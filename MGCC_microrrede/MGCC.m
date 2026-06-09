% --- Script Avançado com Controle por Teclado em Tempo Real ---

% Limpeza inicial
clear; clc; close all; clearvars;
if ~isempty(instrfind)
     fclose(instrfind);
     delete(instrfind);
end

% --- Inicialização do controlador PI SoC---
SoC_ref = 0.6;   % referência desejada
Kp = 150;         % ganho proporcional
Ki = 10;          % ganho integrador
Ts = 0.1;        % tempo de amostragem aproximado em s
integrador = 0;  % inicializa integrador
Pc_max = 200;    % potência máxima de carga (ajuste conforme seu sistema)
Pd_max = 200;    % potência máxima de descarga (ajuste conforme seu sistema)

% --- 1. Configuração da Comunicação Serial ---
port = "COM8"; % !!! MUDE AQUI para a sua porta COM !!!
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
h2_total  = animatedline(ax2,'Color','k','LineWidth',1.5,'DisplayName','Pot\_total');
h2_wt     = animatedline(ax2,'Color','g','LineWidth',1.5,'DisplayName','Pot\_wt');
h2_pv     = animatedline(ax2,'Color','m','LineWidth',1.5,'DisplayName','Pot\_pv');
h2_c     = animatedline(ax2,'Color','b','LineWidth',1.5,'DisplayName','Pot\_carga');
h2_d     = animatedline(ax2,'Color','y','LineWidth',1.5,'DisplayName','Pot\_descarga');
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


% --- 3. Loop de Leitura e Plotagem ---
disp("Iniciando a leitura... ");
startTime = datetime('now');
i = 0;
lastSendTime = 0;  % inicializa
Pot_carga = 0;
Pot_descarga = 0;
ucc = 0;
uel = 0;

while ishandle(fig) % Continua enquanto a figura estiver aberta
    try
        i = i + 1;
        
        % 1. MATLAB envia os controles primeiro
        linha = sprintf('%.1f,%.1f,%.1f,%.1f', Pot_carga, Pot_descarga, ucc, uel);
        writeline(s, linha);

        % 2. MATLAB lê a resposta do Arduino
        data_str = readline(s);
        valores = split(data_str, ',');  % cell array de strings
        valores_num = str2double(valores);  % array de doubles

        nivelAgua(i) = valores_num(1);
        Pot_cc(i)    = valores_num(2);
        Pot_total(i) = valores_num(3);
        SoC(i)       = valores_num(4); 
        Pot_wt(i)    = valores_num(5);
        Pot_pv(i)    = valores_num(6);

        disp(valores_num);

        % Controle simples do nível de água
        if (nivelAgua(i)>=3)
            ucc = 1;
            uel = 0;
        elseif (nivelAgua(i)<=2.5)
            ucc = 0;
            uel = 1;
        end
        
        
        % --- Chamada do controlador PI para SoC ---
        [Pot_carga, Pot_descarga, integrador] = controleSoC(SoC(i), SoC_ref, integrador, Kp, Ki, Ts, Pc_max, Pd_max);

        % Atualiza gráficos
        t = seconds(datetime('now') - startTime);

        % Nível da água
        addpoints(h1, t, nivelAgua(i));
        ax1.XLim = [max(0, t - 50), t + 5];

        % Potências
        addpoints(h2_cc, t, Pot_cc(i));
        addpoints(h2_total, t, Pot_total(i));
        addpoints(h2_wt, t, Pot_wt(i));
        addpoints(h2_pv, t, Pot_pv(i));
        addpoints(h2_d, t, Pot_descarga);
        addpoints(h2_c, t, Pot_carga);
        ax2.XLim = [max(0, t - 50), t + 5];

        % Estado de Carga
        socBar.Position = [-0.4 0 0.8*SoC(i) 1]; % atualiza largura do preenchimento
        if SoC(i) > 0.6
            socBar.FaceColor = 'g';
        elseif SoC(i) > 0.3
            socBar.FaceColor = 'y';
        else
            socBar.FaceColor = 'r';
        end

        drawnow limitrate; % otimiza performance

    catch ME
        if ~isvalid(s)
            disp("A porta serial foi fechada. Encerrando.");
            break;
        end
    end
end




% --- 4. Limpeza ---
clear s;
disp("Comunicação serial encerrada.");


function [Pc, Pd, integrador] = controleSoC(SoC, SoC_ref, integrador, Kp, Ki, Ts, Pc_max, Pd_max)
% Controle PI para manter SoC próximo de SoC_ref
%
% Entradas:
% SoC       - valor atual do SoC
% SoC_ref   - referência desejada do SoC
% integrador- termo integrador acumulado
% Kp, Ki    - ganhos do controlador PI
% Ts        - período de amostragem
% Pc_max, Pd_max - limites máximos de potência de carga/descarga
%
% Saídas:
% Pc, Pd    - potências ajustadas
% integrador- termo integrador atualizado

    erro = SoC_ref - SoC;
    
    % Atualiza integrador
    integrador = integrador + erro*Ts;
    
    % Ação de controle PI
    u = Kp*erro + Ki*integrador;
    
    % Decide carga ou descarga
    if u > 0
        Pc = min(u, Pc_max);  % aplica carga
        Pd = 0;
    else
        Pc = 0;
        Pd = min(-u, Pd_max); % aplica descarga
    end
end