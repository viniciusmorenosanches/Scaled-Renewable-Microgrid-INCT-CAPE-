% MGCC_offline.m
% =========================================================================
%  Simulação Offline da Microrrede INCT-CAPE (100% em Software / In-Silico)
% =========================================================================
%
% Permite simular o cenário de 24 horas e testar o controlador MPC diretamente
% no computador (MATLAB), sem necessidade dos Arduinos ou bancada física.
%
% Vantagens:
%   1. Executa sem necessidade de hardware físico ou portas seriais USB.
%   2. Permite alternar facilmente entre o solver do Vinícius e o da Cecília.
%   3. Suporta modo animado em tempo real (igual à bancada) ou modo turbo (em segundos).
%   4. Gera exatamente os mesmos gráficos e variáveis da bancada física (HIL).
%
% Uso:
%   1. Escolher o solver em SOLVER_CHOICE abaixo ("vini" ou "cecilia").
%   2. Escolher se deseja animação ao vivo em ANIMAR_EM_TEMPO_REAL.
%   3. Executar este script no MATLAB.
%
% =========================================================================

clear; clc; close all;
warning('off', 'MATLAB:nearlySingularMatrix');

% =========================================================================
%  1. CONFIGURAÇÃO — editar aqui antes de rodar
% =========================================================================

% Escolha do Solver MPC:
%   "vini"    -> Solver LPV-PILOT oficial com warm-start (modelo original)
%   "cecilia" -> Novo algoritmo/solver da Cecília (ver função no final)
SOLVER_CHOICE = "vini";

% Animação gráfica em tempo real:
%   true  -> Abre a tela e desenha as curvas e a bateria ao vivo (igual à bancada)
%   false -> Modo turbo: roda todas as 24h em ~3 segundos e plota tudo no final
ANIMAR_EM_TEMPO_REAL = true;

% Inicializa parâmetros físicos e do MPC (Ts=0.1s, k1..k8, matrizes de estado)
microgrid_init_w_demand;

% Parâmetros do cenário
time_to_turn_on = 100;   % passos antes de ativar o MPC (estabilização inicial)
SoC_ref         = 0.6;   % referência de estado de carga da bateria (60%)
ref_h2          = 12.0;  % referência de nível de H2 [mm]

% =========================================================================
%  2. CARREGAR DADOS DO CENÁRIO DE 24 HORAS
% =========================================================================

disp("Carregando cenário 24h (dados_cenario_24h_reformulado.mat)...");
data = load("dados_cenario_24h_reformulado.mat");

N_amostras = min(2400, length(data.demanda));

% Pré-alocação dos vetores de telemetria simulada
nivelAgua        = zeros(1, N_amostras);
SoC_arr          = zeros(1, N_amostras);
Pot_wt_arr       = zeros(1, N_amostras);
Pot_pv_arr       = zeros(1, N_amostras);
Pot_cc_arr       = zeros(1, N_amostras);
Pot_el_arr       = zeros(1, N_amostras);
Pot_total_arr    = zeros(1, N_amostras);
demand           = zeros(1, N_amostras);
dat_pot_carga    = zeros(1, N_amostras);
dat_pot_descarga = zeros(1, N_amostras);

% Condições iniciais
nivelAgua(1) = 5.0;   % nível inicial de H2 [mm]
SoC_arr(1)   = 0.5;   % SoC inicial da bateria [50%]
uel          = 0;     % eletrolisador (0 = desligado, 1 = ligado)
ucc          = 0;     % célula a combustível (0 = desligada, 1 = ligada)
Pot_carga    = 0.0;   % potência de carga da bateria [W]
Pot_descarga = 0.0;   % potência de descarga da bateria [W]

% =========================================================================
%  3. PREPARAÇÃO DA JANELA DE ANIMAÇÃO AO VIVO (SE ATIVADA)
% =========================================================================

if ANIMAR_EM_TEMPO_REAL
    fig = figure('Name', 'Simulação Offline — Monitoramento em Tempo Real', 'NumberTitle', 'off');
    tiledlayout(fig, 3, 1);

    % Subplot 1: Nível de H2
    ax1 = nexttile;
    h1 = animatedline(ax1, 'Color', 'b', 'LineWidth', 1.5);
    ax1.YGrid = 'on';
    title(ax1, 'Nível de H_2');
    xlabel(ax1, 'Tempo Simulado (h)');
    ylabel(ax1, 'Nível (mm)');
    ylim(ax1, [0, 20]);

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
    legend(ax2, 'show', 'Location', 'northeast');
    title(ax2, 'Potências da Microrrede');
    xlabel(ax2, 'Tempo Simulado (h)');
    ylabel(ax2, 'Potência (W)');

    % Subplot 3: Estado de Carga (SoC) — mostrador estilo bateria
    ax3 = nexttile;
    ylim(ax3, [0, 1]);
    xlim(ax3, [-0.5, 0.5]);
    ax3.XTick = [];
    ax3.YTick = [];
    title(ax3, 'Estado de Carga da Bateria (SoC)');
    hold(ax3, 'on');
    rectangle(ax3, 'Position', [-0.4 0 0.8 1],  'EdgeColor', 'k', 'LineWidth', 2);
    rectangle(ax3, 'Position', [-0.1 1 0.2 0.1], 'FaceColor', 'k');
    socBar = rectangle(ax3, 'Position', [-0.4 0 0 1], 'FaceColor', 'white');
    hold(ax3, 'off');

    xlim_max = 24.0;
    xlim(ax1, [0, xlim_max]);
    xlim(ax2, [0, xlim_max]);
end

% =========================================================================
%  4. LOOP PRINCIPAL DE SIMULAÇÃO (24 HORAS)
% =========================================================================

fprintf("Iniciando simulação offline com solver '%s' (Animar ao vivo: %d)...\n", ...
        SOLVER_CHOICE, ANIMAR_EM_TEMPO_REAL);
t_total_start = tic;

for i = 1:N_amostras
    % Interrompe se o usuário fechar a janela durante a animação
    if ANIMAR_EM_TEMPO_REAL && ~ishandle(fig)
        disp("Janela fechada pelo usuário. Encerrando simulação antecipadamente.");
        break;
    end

    % ---------------------------------------------------------------------
    % A. Modelo da Planta: Nível de H2 (integração do tanque físico)
    % ---------------------------------------------------------------------
    if i > 1
        % nH2[k] = nH2[k-1] + k3*uel - k4*ucc
        nivelAgua(i) = nivelAgua(i-1) + k3 * double(uel) - k4 * double(ucc);
        nivelAgua(i) = max(0.0, min(17.0, nivelAgua(i))); % saturação física do copo (17 mm)
    end

    % ---------------------------------------------------------------------
    % B. Fontes Renováveis e Demanda (perfil do cenário)
    % ---------------------------------------------------------------------
    Pot_pv_arr(i) = data.pot_pv(i);
    Pot_wt_arr(i) = data.pot_wt(i);
    demand(i)     = data.demanda(i);

    % ---------------------------------------------------------------------
    % C. Potências dos Atuadores de Hidrogênio
    % ---------------------------------------------------------------------
    Pot_cc_arr(i) = kcc * double(ucc);
    Pot_el_arr(i) = kel * double(uel);

    % ---------------------------------------------------------------------
    % D. Modelo da Planta: SoC da Bateria
    % ---------------------------------------------------------------------
    if i > time_to_turn_on
        SoC_arr(i) = 0.995 * SoC_arr(i-1) + (k1 / 400) * Pot_carga - (k2 / 400) * Pot_descarga;
        SoC_arr(i) = max(0.0, min(1.0, SoC_arr(i)));
    else
        SoC_arr(i) = 1.0; % estabilização inicial
    end

    % ---------------------------------------------------------------------
    % E. Balanço Total de Potência
    % ---------------------------------------------------------------------
    Pot_total_arr(i) = Pot_wt_arr(i) + Pot_pv_arr(i) + Pot_cc_arr(i) ...
                       - Pot_el_arr(i) - Pot_carga + Pot_descarga;

    % ---------------------------------------------------------------------
    % F. Atualização Gráfica em Tempo Real (a cada 10 passos)
    % ---------------------------------------------------------------------
    if ANIMAR_EM_TEMPO_REAL && mod(i, 10) == 0
        t_horas = i * (24.0 / N_amostras);

        addpoints(h1,        t_horas, nivelAgua(i));
        addpoints(h2_cc,     t_horas, Pot_cc_arr(i));
        addpoints(h2_el,     t_horas, Pot_el_arr(i));
        addpoints(h2_total,  t_horas, Pot_total_arr(i));
        addpoints(h2_wt,     t_horas, Pot_wt_arr(i));
        addpoints(h2_pv,     t_horas, Pot_pv_arr(i));
        addpoints(h2_c,      t_horas, Pot_carga);
        addpoints(h2_d,      t_horas, Pot_descarga);
        addpoints(h2_demand, t_horas, demand(i));

        % Atualiza cor e barra da bateria
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

    % ---------------------------------------------------------------------
    % G. Resolução do MPC
    % ---------------------------------------------------------------------
    if i > time_to_turn_on
        % Vetores normalizados para o MPC
        xref       = [SoC_ref; ref_h2 / 10; demand(i) / 1000];
        soc_prev   = max(0.001, min(0.999, SoC_arr(i)));
        nivelAtual = max(0.01, nivelAgua(i));
        x_prev     = [soc_prev; nivelAtual / 10; Pot_total_arr(i) / 1000];
        d          = [Pot_wt_arr(i); Pot_pv_arr(i)];

        if SOLVER_CHOICE == "vini"
            % Solver oficial do Vinícius (LPV-PILOT)
            mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0);
            mpc = update_mpc_precomputed_beq(mpc, x_prev, d, n_rho, mpc.beq0, mpc.beq_ff0);

            [u_sol, J, x0_sol] = mpc_solve(x0, x_prev, u_prev, xref, d, mpc, [], [], []);

            if ~isnan(u_sol)
                u_prev = u_sol;
                x0     = x0_sol;
            else
                % Reinicialização de warm-start caso ocorra inconsistência numérica
                x0 = init_mpc_warm_start(mpc, x_prev, u_prev, d);
            end

        elseif SOLVER_CHOICE == "cecilia"
            % Solver da Cecília
            [u_sol, J] = solver_cecilia(x_prev, u_prev, xref, d, mpc);
            u_prev = u_sol;
        end

        % Desnormalização dos comandos para a planta
        Pot_carga    = u_prev(1) * 400.0;
        Pot_descarga = u_prev(2) * 400.0;
        uel          = (u_prev(3) > 0.5);
        ucc          = (u_prev(4) > 0.5);

        dat_pot_carga(i)    = Pot_carga;
        dat_pot_descarga(i) = Pot_descarga;
    end

    if ~ANIMAR_EM_TEMPO_REAL && mod(i, 400) == 0
        fprintf("  Passo %d / %d (%.1f h simuladas)...\n", i, N_amostras, i * 24 / N_amostras);
    end
end

tempo_execucao = toc(t_total_start);
fprintf("\nSimulação de 24h CONCLUÍDA com sucesso em %.2f segundos!\n", tempo_execucao);

% =========================================================================
%  5. SALVAR RESULTADOS E GERAR GRÁFICOS FINAIS
% =========================================================================

N_pts     = min(i, N_amostras);
SoC       = SoC_arr(1:N_pts);
Pot_cc    = Pot_cc_arr(1:N_pts);
Pot_el    = Pot_el_arr(1:N_pts);
Pot_total = Pot_total_arr(1:N_pts);
Pot_wt    = Pot_wt_arr(1:N_pts);
Pot_pv    = Pot_pv_arr(1:N_pts);
nivelAgua = nivelAgua(1:N_pts);
demand    = demand(1:N_pts);
dat_pot_carga    = dat_pot_carga(1:N_pts);
dat_pot_descarga = dat_pot_descarga(1:N_pts);

arquivo_saida = 'dados_microrrede_offline.mat';
disp("Salvando resultados em " + arquivo_saida + "...");
save(arquivo_saida, ...
     'nivelAgua', 'SoC', 'Pot_cc', 'Pot_el', 'Pot_total', ...
     'Pot_wt', 'Pot_pv', 'Pot_carga', 'Pot_descarga', 'demand', ...
     'dat_pot_carga', 'dat_pot_descarga');
disp("Resultados salvos.");

% Plota gráficos finais detalhados de 4 subplots
plotar_resultados_mgcc(arquivo_saida, SoC_ref, ref_h2, N_pts);

% =========================================================================
%  6. FUNÇÃO DO SOLVER DA CECÍLIA (ESPAÇO RESERVADO PARA ADAPTAÇÃO)
% =========================================================================
function [u_sol, J] = solver_cecilia(x_prev, u_prev, xref, d, mpc)
    % solver_cecilia — Espaço para a Cecília plugar seu algoritmo de otimização.
    %
    % Entradas:
    %   x_prev — Estado atual [SoC; Nivel_H2/10; Pot_total/1000]
    %   u_prev — Controle do passo anterior [Pc; Pd; uel; ucc] normalizados (0..1)
    %   xref   — Referências [SoC_ref; Nivel_ref/10; Demand/1000]
    %   d      — Perturbações renováveis [Pot_wt; Pot_pv] em Watts
    %   mpc    — Estrutura de dados com matrizes do sistema e restrições
    %
    % Saída:
    %   u_sol  — Vetor de 4 comandos normalizados [Pc; Pd; uel; ucc] em [0, 1]
    %   J      — Custo da função objetivo (opcional)

    % --- Exemplo / Fallback provisório: repassa o solver oficial enquanto ---
    % --- a Cecília insere seu próprio solver aqui                         ---
    [u_sol, J, ~] = mpc_solve([], x_prev, u_prev, xref, d, mpc, [], [], []);
end
