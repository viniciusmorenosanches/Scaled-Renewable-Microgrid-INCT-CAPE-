% MGCC_w_demand.m
% Script principal do MGCC (Microgrid Central Controller) com MPC e demanda variável.
%
% Executa o cenário de 24h: lê perfis climáticos (solar/eólico) e de demanda,
% envia referências ao Arduino R4 via serial, lê telemetria, chama o solver
% MPC a cada 100 ms e plota os resultados em tempo real.
%
% Uso:
%   1. Confirmar a porta serial em COM_PORT abaixo.
%   2. Ligar todos os Arduinos e aguardar o ESC da turbina armar (~10 s).
%   3. Executar este script no MATLAB.
%
% Dependências:
%   microgrid_init_w_demand.m   — configuração do MPC (executada automaticamente)
%   LPV-PILOT-final_features/   — solver MPC (biblioteca externa, não modificar)
%   referenciaSol.csv           — perfil de geração solar (2400 pontos, 1 por passo)
%   referenciaVento.csv         — perfil de geração eólica (2400 pontos, 1 por passo)
%   dados_cenario_24h_reformulado.mat — perfil de demanda elétrica de 24h
%
% Saída:
%   dados_microrrede_test.mat   — dados da simulação (sobrescrito a cada execução)

% =========================================================================
%  Limpeza e inicialização
% =========================================================================

clear; clc; close all;
warning('off', 'MATLAB:nearlySingularMatrix');

% =========================================================================
%  CONFIGURAÇÃO — editar aqui antes de rodar
% =========================================================================

COM_PORT = "COM4";   % porta serial do Arduino R4 (verificar no Gerenciador de Dispositivos)

% Inicializa o MPC (carrega: mpc, x0, u_prev, Bd0, n_rho, Ts, kcc, kel, ...)
microgrid_init_w_demand

% Parâmetros do controlador H2 (heurística de nível)
time_to_turn_on = 100;   % iterações antes de ativar o MPC (deixa o sistema estabilizar)
SoC_ref = 0.6;           % referência de estado de carga da bateria
ref_h2  = 12;            % referência de nível de H2 [mm]

% =========================================================================
%  Carregamento dos perfis climáticos e de demanda
% =========================================================================

disp("Carregando dados do cenário de 24h...");
data = load("dados_cenario_24h_reformulado.mat");

try
    array_ref_sol   = readmatrix('referenciaSol.csv');
    array_ref_vento = readmatrix('referenciaVento.csv');
catch
    error("Arquivos CSV de referência não encontrados. Certifique-se de executar o MATLAB na pasta MGCC_microrrede/.");
end

% =========================================================================
%  Comunicação serial com o Arduino R4
% =========================================================================

s = serialport(COM_PORT, 115200);
configureTerminator(s, "LF");
disp("Conectado ao Arduino na porta " + COM_PORT);

% Garante o desligamento imediato e seguro de todos os atuadores (turbina, painel, H2)
% caso o script seja interrompido (Ctrl+C), a janela fechada ou ocorra erro
cleanupObj = onCleanup(@() shutdown_microgrid(s));

% Aguarda o Arduino R4 inicializar após o auto-reset da porta serial (~2 s)
disp("Aguardando inicialização do Arduino R4...");
pause(2.0);
flush(s);

% =========================================================================
%  Preparação da figura de monitoramento em tempo real
% =========================================================================

fig = figure('Name', 'Monitoramento Microrrede — Cenário 24h', 'NumberTitle', 'off');
tiledlayout(fig, 3, 1);

% Subplot 1: Nível de H2
ax1 = nexttile;
h1 = animatedline(ax1, 'Color', 'b');
ax1.YGrid = 'on';
title(ax1, 'Nível de H_2');
xlabel(ax1, 'Tempo (s)');
ylabel(ax1, 'Nível (mm)');

% Subplot 2: Potências
ax2 = nexttile;
h2_cc     = animatedline(ax2, 'Color', 'r',    'LineWidth', 1.5, 'DisplayName', 'Pot\_cc');
h2_el     = animatedline(ax2, 'Color', 'c',    'LineWidth', 1.5, 'DisplayName', 'Pot\_el');
h2_total  = animatedline(ax2, 'Color', 'k',    'LineWidth', 1.5, 'DisplayName', 'Pot\_total');
h2_wt     = animatedline(ax2, 'Color', 'g',    'LineWidth', 1.5, 'DisplayName', 'Pot\_wt');
h2_pv     = animatedline(ax2, 'Color', 'm',    'LineWidth', 1.5, 'DisplayName', 'Pot\_pv');
h2_c      = animatedline(ax2, 'Color', 'b',    'LineWidth', 1.5, 'DisplayName', 'Pot\_carga');
h2_d      = animatedline(ax2, 'Color', 'y',    'LineWidth', 1.5, 'DisplayName', 'Pot\_descarga');
h2_demand = animatedline(ax2, 'Color', 'cyan', 'LineWidth', 1.0, 'DisplayName', 'Demanda');
ax2.YGrid = 'on';
legend(ax2, 'show');
title(ax2, 'Potências');
xlabel(ax2, 'Tempo (s)');
ylabel(ax2, 'Potência (W)');

% Subplot 3: Estado de Carga (SoC) — mostrador de bateria
ax3 = nexttile;
ylim(ax3, [0 1]);
xlim(ax3, [-0.5 0.5]);
ax3.XTick = [];
ax3.YTick = [];
title(ax3, 'Estado de Carga da Bateria');
hold(ax3, 'on');
rectangle(ax3, 'Position', [-0.4 0 0.8 1],  'EdgeColor', 'k', 'LineWidth', 2);
rectangle(ax3, 'Position', [-0.1 1 0.2 0.1], 'FaceColor', 'k');
socBar = rectangle(ax3, 'Position', [-0.4 0 0 1], 'FaceColor', 'white');
hold(ax3, 'off');

xlim_max = 55;
xlim(ax1, [0, xlim_max]);
xlim(ax2, [0, xlim_max]);

% =========================================================================
%  Inicialização das variáveis de estado do loop
% =========================================================================

i             = 0;
nivelAgua     = zeros(1, 2400);
SoC_arr       = zeros(1, 2400);
Pot_cc_arr    = zeros(1, 2400);
Pot_el_arr    = zeros(1, 2400);
Pot_total_arr = zeros(1, 2400);
Pot_wt_arr    = zeros(1, 2400);
Pot_pv_arr    = zeros(1, 2400);
demand        = zeros(1, 2400);
dat_pot_carga    = zeros(1, 2400);
dat_pot_descarga = zeros(1, 2400);

nivelAgua(1) = 5;
SoC_arr(1)   = 0.5;
uel          = 0;
ucc          = 0;
Pot_carga    = 0;
Pot_descarga = 0;

total_loop_start = tic;
dt_write = 0; dt_read = 0; dt_plot = 0; dt_mpc = 0; dt_total_paced = 0;

disp("Iniciando cenário de 24h (2400 iterações de 100 ms = 240 s reais)...");
startTime = datetime('now');

% =========================================================================
%  Loop principal — 10 Hz, 2400 iterações (= 24h simuladas)
% =========================================================================

while ishandle(fig)
    i = i + 1;
    if i > 2400
        disp("Cenário de 24 horas concluído com sucesso!");
        break;
    end

    try
        iterStart = tic;

        % ------------------------------------------------------------------
        % 1. Ler referências climáticas do perfil para este passo
        % ------------------------------------------------------------------
        if i <= length(array_ref_sol)
            ref_sol_volts = array_ref_sol(i);
        else
            ref_sol_volts = 0;
        end

        if i <= length(array_ref_vento)
            ref_vento_volts = array_ref_vento(i);
        else
            ref_vento_volts = 0;
        end

        % ------------------------------------------------------------------
        % 2. Enviar comandos e referências ao Arduino R4
        %    Formato: Pc, Pd, ucc, uel, ref_sol, ref_vento  (CSV, 6 campos)
        % ------------------------------------------------------------------
        t_write = tic;
        linha_envio = sprintf('%.2f,%.2f,%d,%d,%.2f,%.2f', ...
                              Pot_carga, Pot_descarga, ucc, uel, ref_sol_volts, ref_vento_volts);
        writeline(s, linha_envio);
        dt_write = toc(t_write);

        % ------------------------------------------------------------------
        % 3. Ler telemetria do Arduino R4
        %    Formato: Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd
        % ------------------------------------------------------------------
        t_read = tic;
        data_str  = readline(s);
        dt_read   = toc(t_read);

        if mod(i, 10) == 0
            disp("RAW DATA DO R4: " + data_str);
        end

        % Divide os valores recebidos do R4.
        % NOTA DE INTEGRAÇÃO: O R4 envia 8 valores via telemetria:
        % [Level_H2, Pot_cc, Pot_total, SoC, Pot_wt, Pot_pv, Pc, Pd]
        % Por decisão de projeto, o MATLAB utiliza do R4 apenas o nível de H2 (1),
        % Pot_wt (5) e Pot_pv (6). O SoC e a Pot_cc são calculados localmente
        % no modelo MATLAB para maior precisão e estabilidade.
        valores_num = str2double(split(data_str, ','));

        nivelAgua(i) = valores_num(1);
        Pot_wt_arr(i) = valores_num(5);
        Pot_pv_arr(i) = valores_num(6);
        demand(i)     = data.demanda(i);

        % Potências dos atuadores H2 calculadas localmente (mais estável que ler do R4)
        Pot_cc_arr(i) = kcc * ucc;
        Pot_el_arr(i) = kel * uel;

        % Modelo de SoC calculado no MATLAB (modelo discreto, mais preciso que o do R4)
        if i > time_to_turn_on
            SoC_arr(i) = 0.995 * SoC_arr(i-1) + (k1/400) * Pot_carga - (k2/400) * Pot_descarga;
            SoC_arr(i) = max(0, min(1, SoC_arr(i)));   % saturação [0, 1]
        else
            SoC_arr(i) = 1;   % aguarda estabilização antes de controlar
        end

        Pot_total_arr(i) = Pot_wt_arr(i) + Pot_pv_arr(i) + Pot_cc_arr(i) ...
                           - Pot_el_arr(i) - Pot_carga + Pot_descarga;

        if mod(i, 10) == 0
            fprintf('Iter %d | Pot_pv=%.1f W | Pot_wt=%.1f W | SoC=%.3f | H2=%.1f mm\n', ...
                    i, Pot_pv_arr(i), Pot_wt_arr(i), SoC_arr(i), nivelAgua(i));
        end

        % ------------------------------------------------------------------
        % 4. Atualização dos gráficos (a cada 10 passos para reduzir custo)
        % ------------------------------------------------------------------
        t_plot = tic;
        if mod(i, 10) == 0
            t = seconds(datetime('now') - startTime);
            addpoints(h1,        t, nivelAgua(i));
            addpoints(h2_cc,     t, Pot_cc_arr(i));
            addpoints(h2_el,     t, Pot_el_arr(i));
            addpoints(h2_total,  t, Pot_total_arr(i));
            addpoints(h2_wt,     t, Pot_wt_arr(i));
            addpoints(h2_pv,     t, Pot_pv_arr(i));
            addpoints(h2_d,      t, Pot_descarga);
            addpoints(h2_c,      t, Pot_carga);
            addpoints(h2_demand, t, demand(i));

            if t > xlim_max - 5
                xlim_max = xlim_max + 15;
                xlim(ax1, [max(0, xlim_max - 55), xlim_max]);
                xlim(ax2, [max(0, xlim_max - 55), xlim_max]);
            end

            % Atualiza mostrador de SoC
            socBar.Position = [-0.4 0 0.8 * SoC_arr(i) 1];
            if SoC_arr(i) > 0.6
                socBar.FaceColor = 'g';
            elseif SoC_arr(i) > 0.3
                socBar.FaceColor = 'y';
            else
                socBar.FaceColor = 'r';
            end

            drawnow limitrate;
        end
        dt_plot = toc(t_plot);

        % ------------------------------------------------------------------
        % 5. Resolver o MPC — calcula Pc, Pd, uel, ucc para o próximo passo
        % ------------------------------------------------------------------
        t_mpc = tic;
        if i > time_to_turn_on
            xref  = [SoC_ref; ref_h2 / 10; demand(i) / 1000];
            if isnan(nivelAgua(i))
                nivelAtual = ref_h2;
            else
                nivelAtual = nivelAgua(i);
            end
            soc_prev = max(0.001, min(0.999, SoC_arr(i)));
            nivelAtual = max(0.01, nivelAtual);
            x_prev = [soc_prev; nivelAtual / 10; Pot_total_arr(i) / 1000];
            d      = [Pot_wt_arr(i); Pot_pv_arr(i)];

            mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0);
            mpc = update_mpc_precomputed_beq(mpc, x_prev, d, n_rho, mpc.beq0, mpc.beq_ff0);

            [u_sol, J, x0_sol] = mpc_solve(x0, x_prev, u_prev, xref, d, mpc, [], [], []);

            if ~isnan(u_sol)
                u_prev = u_sol;
                x0     = x0_sol;
                
                Pot_carga    = u_prev(1) * 400;
                Pot_descarga = u_prev(2) * 400;
                uel = (u_prev(3) > 0.5);
                ucc = (u_prev(4) > 0.5);

                dat_pot_carga(i)    = Pot_carga;
                dat_pot_descarga(i) = Pot_descarga;
            else
                disp("MPC retornou NaN na iteração " + i + " — reinicializando warm-start e mantendo último controle.");
                u_prev = [Pot_carga / 400; Pot_descarga / 400; double(uel); double(ucc)];
                x0     = init_mpc_warm_start(mpc, x_prev, u_prev, d);
            end
        end
        dt_mpc = toc(t_mpc);

        % ------------------------------------------------------------------
        % 6. Pacing: aguardar para manter exatamente 10 Hz
        %    Corrige drift cumulativo para que i*Ts corresponda ao tempo real
        % ------------------------------------------------------------------
        target_time    = i * Ts;
        elapsed_total  = toc(total_loop_start);
        time_to_pause  = target_time - elapsed_total;
        if time_to_pause > 0
            pause(time_to_pause);
        end
        dt_total_paced = toc(iterStart);

    catch ME
        fprintf('Erro na iteração %d: %s\n', i, ME.message);
        if ~isvalid(s)
            disp("Porta serial fechada. Encerrando.");
            break;
        end
    end

    if mod(i, 10) == 0
        fprintf('--- TIMING (iter %d) ---\n', i);
        fprintf('  Write: %.1f ms | Read: %.1f ms | Plot: %.1f ms | MPC: %.1f ms | Total: %.1f ms\n', ...
                dt_write*1000, dt_read*1000, dt_plot*1000, dt_mpc*1000, dt_total_paced*1000);
        fprintf('  Tempo simulado: %.2f h | Tempo real: %.1f s\n', i*0.01, toc(total_loop_start));
    end
end

% =========================================================================
%  Salvar dados e gerar gráficos finais
% =========================================================================

N_pts = min(i, 2400);

% Mapeia de volta para nomes de variáveis originais para retrocompatibilidade
SoC       = SoC_arr;
Pot_cc    = Pot_cc_arr;
Pot_el    = Pot_el_arr;
Pot_total = Pot_total_arr;
Pot_wt    = Pot_wt_arr;
Pot_pv    = Pot_pv_arr;

disp("Salvando dados em dados_microrrede_test.mat...");
save('dados_microrrede_test.mat', ...
     'nivelAgua', 'SoC', 'Pot_cc', 'Pot_el', 'Pot_total', ...
     'Pot_wt', 'Pot_pv', 'Pot_carga', 'Pot_descarga', 'demand', ...
     'dat_pot_carga', 'dat_pot_descarga');
disp("Dados salvos.");

% Envia comando seguro de parada para zerar turbina, painel e H2 antes de fechar a porta
shutdown_microgrid(s);
clear s;
disp("Comunicação serial encerrada.");

%% Gráficos finais
plotar_resultados_mgcc('dados_microrrede_test.mat', SoC_ref, ref_h2, N_pts);

% =========================================================================
%  Função Auxiliar de Desligamento Seguro
% =========================================================================
function shutdown_microgrid(serialPort)
    if isvalid(serialPort)
        try
            % Envia zeros para todos os comandos: Pc=0, Pd=0, ucc=0, uel=0, ref_sol=0, ref_vento=0
            writeline(serialPort, "0.00,0.00,0,0,0.00,0.00");
            pause(0.08);
            disp(">> [SEGURANÇA] Comandos zerados e atuadores desligados com sucesso.");
        catch
        end
    end
end