% --- Script Avançado com Controle por Teclado em Tempo Real ---

% Limpeza inicial
clear; clc; close all; clearvars;
warning('off', 'MATLAB:nearlySingularMatrix');

microgrid_init_w_demand

data = load("dados_cenario_24h_reformulado.mat");

% --- Carregando as referências climáticas externas ---
disp("Carregando perfis climáticos (CSV)...");
try
    array_ref_sol = readmatrix('referenciaSol.csv');
    array_ref_vento = readmatrix('referenciaVento.csv');
catch
    disp("Erro: Arquivos CSV não encontrados! Certifique-se de que estão na mesma pasta.");
    return;
end

% if ~isempty(instrfind)
%      fclose(instrfind);
%      delete(instrfind);
% end

k1 = 3.92e-5 * 400;
k2 = 4.08e-5 * 400;
%k3 = 130e-9;
k3 = 0.1 * Ts; % mm/s
%k4 = 82.63e-9;
k4 = 0.0658 * Ts; % mm/s

time_to_turn_on = 100;

% --- Inicialização do controlador PI SoC---
SoC_ref = 0.6;   % referência desejada
ref_h2 = 12;
Kp = 150;         % ganho proporcional
Ki = 10;          % ganho integrador
Ts = 0.1;        % tempo de amostragem aproximado em s
integrador = 0;  % inicializa integrador
Pc_max = 400;    % potência máxima de carga (ajuste conforme seu sistema)
Pd_max = 400;    % potência máxima de descarga (ajuste conforme seu sistema)


kcc = 154.57;
kel = 10; % talvez seja 100
uel = 0;
ucc = 0;

% --- 1. Configuração da Comunicação Serial ---
port = "COM9"; % !!! MUDE AQUI para a sua porta COM !!!
baudRate = 115200;
s = serialport(port, baudRate);
configureTerminator(s, "LF"); % Define o terminador de linha
disp("Conectado ao Arduino na porta " + port);

% --- 2. Preparação da Figura e Subplots ---
fig = figure('Name','Monitoramento Microrrede','NumberTitle','off');
tiledlayout(fig,3,1)  % 3 linhas, 1 coluna

% Subplot 1: Nível da água
ax1 = nexttile;
h1 = animatedline(ax1,'Color','b');
ax1.YGrid = 'on';
title(ax1,'Nível de H2');
xlabel(ax1,'Tempo (s)');
ylabel(ax1,'Nível (mm)');

% Subplot 2: Potências
ax2 = nexttile;
h2_cc     = animatedline(ax2,'Color','r','LineWidth',1.5,'DisplayName','Pot\_cc');
h2_el     = animatedline(ax2,'Color','c','LineWidth',1.5,'DisplayName','Pot\_el');
h2_total  = animatedline(ax2,'Color','k','LineWidth',1.5,'DisplayName','Pot\_total');
h2_wt     = animatedline(ax2,'Color','g','LineWidth',1.5,'DisplayName','Pot\_wt');
h2_pv     = animatedline(ax2,'Color','m','LineWidth',1.5,'DisplayName','Pot\_pv');
h2_c     = animatedline(ax2,'Color','b','LineWidth',1.5,'DisplayName','Pot\_carga');
h2_d     = animatedline(ax2,'Color','y','LineWidth',1.5,'DisplayName','Pot\_descarga');
h2_demand = animatedline(ax2,'Color','cyan','LineWidth',1.0,'DisplayName','demand');
ax2.YGrid = 'on';
legend(ax2,'show');
title(ax2,'Potências');
xlabel(ax2,'Tempo (s)');
ylabel(ax2,'Potência (W)');

% Subplot 3: Estado de Carga (SoC) com mostrador de bateria
ax3 = nexttile;
%batteryBar = bar(ax3,0,0.5,'FaceColor','g'); % barra inicial
ylim(ax3,[0 1]);
xlim(ax3,[-0.5 0.5]);
ax3.XTick = [];
ax3.YTick = [];
title(ax3,'Estado de Carga da Bateria');
% Desenha contorno da bateria
hold(ax3,'on');
rectangle(ax3,'Position',[-0.4 0 0.8 1],'EdgeColor','k','LineWidth',2); % corpo
rectangle(ax3,'Position',[-0.1 1 0.2 0.1],'FaceColor','k'); % terminal
socBar = rectangle(ax3,'Position',[-0.4 0 0 1],'FaceColor','white'); % preenchimento inicial
hold(ax3,'off');

xlim_max = 55;
xlim(ax1, [0, xlim_max]);
xlim(ax2, [0, xlim_max]);

% --- 3. Loop de Leitura e Plotagem ---
disp("Iniciando a leitura... ");
startTime = datetime('now');
i = 0;
lastSendTime = 0;  % inicializa
dt_mpc = 0;
nivelAgua(1) = 5;
SoC(1) = 0.5;
uel = 0;
ucc = 0;
Pot_carga = 0;
Pot_descarga = 0;

demand(1) = 0;
dt_mpc = 0;
total_loop_start = tic;
dt_total_paced = 0;


while ishandle(fig) % Continua enquanto a figura estiver aberta
    i = i + 1;
    if i > 2400
        disp("Cenário de 24 horas concluído com sucesso!");
        break;
    end
    try
        iterStart = tic;
        
        % --------------------------------------------------
        % 1. MATLAB MANDA AS ORDENS E REFERÊNCIAS PRIMEIRO
        % --------------------------------------------------
        t_write = tic;
        if i <= length(array_ref_sol)
            ref_sol_volts = array_ref_sol(i);
        else
            ref_sol_volts = 0; % Desliga se o cenário acabar
        end

        % Lê a referência de Vento do array
        if i <= length(array_ref_vento)
            ref_vento_volts = array_ref_vento(i);
        else
            ref_vento_volts = 0; 
        end

        % Monta o pacote com as 6 variáveis para enviar ao Arduino
        linha_envio = sprintf('%.2f,%.2f,%d,%d,%.2f,%.2f', Pot_carga, Pot_descarga, ucc, uel, ref_sol_volts, ref_vento_volts);
        writeline(s, linha_envio);
        dt_write = toc(t_write);
        
        % --------------------------------------------------
        % 2. MATLAB LÊ A RESPOSTA (O Arduino responde imediatamente)
        % -------------------------------------------------- 
        t_read = tic;
        data_str = readline(s);
        dt_read = toc(t_read);

        if mod(i, 10) == 0
            disp("RAW DATA DO R4: " + data_str);
        end

        valores = split(data_str, ',');  % cell array de strings
        valores_num = str2double(valores);  % array de doubles

        nivelAgua(i) = valores_num(1);

        Pot_cc(i) = kcc*ucc;

        Pot_el(i) = kel*uel;
        
        if i > time_to_turn_on
            SoC(i) = 0.995*SoC(i - 1) + k1/400*Pot_carga - k2/400*Pot_descarga;
            if SoC(i) < 0
                SoC(i) = 0;
            elseif SoC(i) > 1
                SoC(i) = 1;
            end
        else
            SoC(i) = 1;
        end
        
        Pot_wt(i)    = valores_num(5);
        Pot_pv(i)    = valores_num(6);

        demand(i) = data.demanda(i);

        if mod(i, 10) == 0
            fprintf('Iteração: %d | Pot_pv: %.2f W | Pot_wt: %.2f W | SoC: %.2f | Nível H2: %.2f\n', ...
                     i, Pot_pv(i), Pot_wt(i), SoC(i), nivelAgua(i));
        end
        
        Pot_total(i) = Pot_wt(i) + Pot_pv(i) + Pot_cc(i) - Pot_el(i) - Pot_carga + Pot_descarga;

        % Atualiza dados gráficos (operação lenta, feita a cada 10 passos)
        t_plot = tic;
        t = seconds(datetime('now') - startTime);
        if mod(i, 10) == 0
            % Adiciona pontos apenas nos instantes de renderização (reduz o custo de desenho por 10x)
            addpoints(h1, t, nivelAgua(i));
            addpoints(h2_cc, t, Pot_cc(i));
            addpoints(h2_el, t, Pot_el(i));
            addpoints(h2_total, t, Pot_total(i));
            addpoints(h2_wt, t, Pot_wt(i));
            addpoints(h2_pv, t, Pot_pv(i));
            addpoints(h2_d, t, Pot_descarga);
            addpoints(h2_c, t, Pot_carga);
            addpoints(h2_demand, t, demand(i));

            if t > xlim_max - 5
                xlim_max = xlim_max + 15;
                xlim(ax1, [max(0, xlim_max - 55), xlim_max]);
                xlim(ax2, [max(0, xlim_max - 55), xlim_max]);
            end

            % Estado de Carga
            socBar.Position = [-0.4 0 0.8*SoC(i) 1]; % atualiza largura do preenchimento
            if SoC(i) > 0.6
                socBar.FaceColor = 'g';
            elseif SoC(i) > 0.3
                socBar.FaceColor = 'y';
            else
                socBar.FaceColor = 'r';
            end

            drawnow limitrate;
        end
        dt_plot = toc(t_plot);

        % 4. Calculo MPC (incluido dentro do pacing da iteracao)
        t_mpc = tic;
        if i > time_to_turn_on
            xref = [SoC_ref;ref_h2/10;demand(end)/1000];

            if isnan(nivelAgua(end))
                nivelAgua(end) = ref_h2;
            end
        
            x_prev = [SoC(end); nivelAgua(end)/10; Pot_total(end)/1000];
        
            d = [Pot_wt(end); Pot_pv(end)];

            mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0);
            mpc = update_mpc_precomputed_beq(mpc, x_prev, d, n_rho, ...
                mpc.beq0, mpc.beq_ff0);
        
            [u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);
        
            if isnan(u_prev)
                disp("Solution is NaN!")
            else
                Pot_carga = u_prev(1) * 400;
                Pot_descarga = u_prev(2) * 400;

                dat_pot_carga(i) = Pot_carga;
                dat_pot_descarga(i) = Pot_descarga;
            
                if u_prev(3) > 0.5
                    uel = 1;
                else
                    uel = 0;
                end
            
                if u_prev(4) > 0.5
                    ucc = 1;
                else
                    ucc = 0;
                end
            end
        end
        dt_mpc = toc(t_mpc);

        % 5. Pacing inteligente cumulativo para manter exatamente 10Hz (Opcao B com correcao de drift)
        target_time = i * Ts;
        elapsed_total = toc(total_loop_start);
        time_to_pause = target_time - elapsed_total;
        if time_to_pause > 0
            pause(time_to_pause);
        end
        dt_total_paced = toc(iterStart);

    catch ME
        fprintf('Erro na iteração %d: %s\n', i, ME.message);
        if ~isvalid(s)
            disp("A porta serial foi fechada. Encerrando.");
            break;
        end
    end

    if mod(i, 10) == 0
        fprintf('--- TIMING REPORT (Iteracao %d) ---\n', i);
        fprintf('1. Escrita Serial (Write): %.2f ms\n', dt_write * 1000);
        fprintf('2. Leitura Serial (Read) : %.2f ms\n', dt_read * 1000);
        fprintf('3. Atualizacao Grafica   : %.2f ms\n', dt_plot * 1000);
        fprintf('4. Calculo MPC           : %.2f ms\n', dt_mpc * 1000);
        fprintf('5. Tempo Real do Passo   : %.2f ms (Raw: %.2f ms)\n', dt_total_paced * 1000, (dt_write + dt_read + dt_plot + dt_mpc) * 1000);
        fprintf('6. Tempo Real Acumulado  : %.2f segundos\n', toc(total_loop_start));
        fprintf('7. Horas de Simulacao    : %.2f horas\n', i * 0.01);
        fprintf('-----------------------------------\n\n');
    end
end
%%
disp("Salvando dados em dados_microrrede.mat...");
save('dados_microrrede_test.mat', ...
     'nivelAgua', 'SoC', ...
     'Pot_cc', 'Pot_el', 'Pot_total', 'Pot_wt', 'Pot_pv', ...
     'Pot_carga', 'Pot_descarga', 'demand', 'dat_pot_carga', 'dat_pot_descarga');
disp("Dados salvos com sucesso.");
%%
close all
dat = load('dados_microrrede_test.mat');

SoC_ref = 0.6;
ref_h2 = 5;

% Determine minimum array length to handle early stops gracefully
N_pts = min([length(dat.Pot_total), length(dat.demand), length(dat.Pot_wt), length(dat.Pot_pv), length(dat.nivelAgua), ...
             length(dat.SoC), length(dat.dat_pot_carga), length(dat.dat_pot_descarga), length(dat.Pot_el), length(dat.Pot_cc)]);
N_pts = min(N_pts, 2390);

if N_pts < 2
    disp("Poucos dados salvos para gerar os graficos.");
    return;
end

dat.Pot_total = dat.Pot_total(1:N_pts);
dat.demand = dat.demand(1:N_pts);
dat.Pot_wt = dat.Pot_wt(1:N_pts);
dat.Pot_pv = dat.Pot_pv(1:N_pts);
dat.nivelAgua = dat.nivelAgua(1:N_pts);
dat.SoC = dat.SoC(1:N_pts);
dat.dat_pot_carga = dat.dat_pot_carga(1:N_pts);
dat.dat_pot_descarga = dat.dat_pot_descarga(1:N_pts);
dat.Pot_el = dat.Pot_el(1:N_pts);
dat.Pot_cc = dat.Pot_cc(1:N_pts);

for i=2:length(dat.Pot_total)
    if dat.Pot_total(i) == 0
        dat.Pot_total(i) = dat.Pot_total(i-1);
    end
    if dat.demand(i) == 0
        dat.demand(i) = dat.demand(i-1);
    end
    if dat.Pot_wt(i) == 0 && i > 1
        dat.Pot_wt(i) = dat.Pot_wt(i-1);
    end
    if dat.Pot_pv(i) == 0
        dat.Pot_pv(i) = dat.Pot_pv(i-1);
    end
end

% tratando dados
dat.Pot_total = max(50, dat.Pot_total);

for j = 2:length(dat.Pot_wt)
    if dat.Pot_wt(j) < 50
        dat.Pot_wt(j) = dat.Pot_wt(j - 1);
    end
end

time = (0:N_pts-1) * (24 / 2400); % time in simulated hours

figure
subplot(4,1,1)
plot(time(1:2:end),dat.Pot_total(1:2:end), 'LineWidth', 1.5)
hold on
plot(time(1:2:end),dat.demand(1:2:end), 'LineWidth', 1.5)
legend('P_{total}(k)', 'P_{demand}(k)', 'FontSize', 12)
ylabel('Power (W)', 'FontSize', 14)
%xlabel('Time (h)', 'FontSize', 14)
xlim([0,24])
grid on

subplot(4,1,2)
plot(time(1:2:end),dat.Pot_wt(1:2:end), 'LineWidth', 2.0)
hold on

dat.Pot_pv = movmean(dat.Pot_pv, 100);
plot(time(1:2:end), dat.Pot_pv(1:2:end), 'LineWidth', 2.0)

legend('P_{wt}(k)', 'P_{pv}(k)', 'FontSize', 12)
ylabel('Renewable\newlineEnergy (W)', 'FontSize', 14)
%xlabel('Time (h)', 'FontSize', 14)
xlim([0,24])
grid on

subplot(4,1,3)
plot(time(1:2:end),dat.SoC(1:2:end), 'LineWidth', 2.0)
hold on
plot(time(1:2:end), SoC_ref*ones(size(time(1:2:end))), 'LineWidth', 2.0, 'LineStyle','--', 'Color', 'r')
legend('SoC(k)', 'SoC_{ref}(k)', 'FontSize', 12)
ylabel('SoC (%)', 'FontSize', 14)
%xlabel('Time (h)')
xlim([0,24])
grid on

subplot(4,1,4)
plot(time(1:2:end),dat.nivelAgua(1:2:end), 'LineWidth', 2.0)
hold on
plot(time(1:2:end), ref_h2*ones(size(time(1:2:end))), 'LineWidth', 2.0, 'LineStyle','--', 'Color', 'r')
legend('H_2', 'H_2_{ref}', 'FontSize', 12)
ylabel('Level of H_2\newline(mm)', 'FontSize', 14)
xlabel('Time (h)', 'FontSize', 14)
xlim([0,24])
grid on

%%
figure
subplot(2,1,1)
plot(time,-dat.dat_pot_carga + dat.dat_pot_descarga, 'LineWidth', 2.0)
legend('P_{discharge} (>0) / P_{charge} (<0)', 'FontSize', 12)
ylabel('Battery\newlinecommand\newline(W)', 'FontSize', 14)
%xlabel('Time (h)')
xlim([0,24])
grid on

subplot(2,1,2)
dat.Pot_el = min(1,dat.Pot_el);
dat.Pot_cc = min(1,dat.Pot_cc);
stairs(time,- dat.Pot_el + dat.Pot_cc, 'LineWidth', 2.0)
%hold on
%stairs(time,dat.Pot_cc, 'LineWidth', 2.0)
legend('u_{fc} (>0) / u_{el} (<0)', 'FontSize', 12)
ylabel('Electrol./\newlineFuel cell', 'FontSize', 14)
xlabel('Time (h)', 'FontSize', 14)
xlim([0,24])
grid on

% --- 4. Limpeza ---
clear s;
disp("Comunicação serial encerrada.");


% function [Pc, Pd, integrador] = controleSoC(SoC, SoC_ref, integrador, Kp, Ki, Ts, Pc_max, Pd_max)
% % Controle PI para manter SoC próximo de SoC_ref
% %
% % Entradas:
% % SoC       - valor atual do SoC
% % SoC_ref   - referência desejada do SoC
% % integrador- termo integrador acumulado
% % Kp, Ki    - ganhos do controlador PI
% % Ts        - período de amostragem
% % Pc_max, Pd_max - limites máximos de potência de carga/descarga
% %
% % Saídas:
% % Pc, Pd    - potências ajustadas
% % integrador- termo integrador atualizado
% 
%     erro = SoC_ref - SoC;
% 
%     % Atualiza integrador
%     integrador = integrador + erro*Ts;
% 
%     % Ação de controle PI
%     u = Kp*erro + Ki*integrador;
% 
%     % Decide carga ou descarga
%     if u > 0
%         Pc = min(u, Pc_max);  % aplica carga
%         Pd = 0;
%     else
%         Pc = 0;
%         Pd = min(-u, Pd_max); % aplica descarga
%     end
% end