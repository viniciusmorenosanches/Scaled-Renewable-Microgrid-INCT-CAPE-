% calibrar_h2.m
% Script de calibração interativa para o sistema de Hidrogênio (H2).
%
% Este script calcula:
%   1. A faixa de nível de água (nível mínimo para H2=0%, nível máximo para H2=100%).
%   2. A taxa de enchimento pelo eletrolisador (k3) em mm/s e sua variação temporal.
%   3. A taxa de perda passiva do tanque em mm/s.
%   4. A taxa de consumo pela célula (k4) em mm/s e sua variação temporal.
%
% Segurança: Pressionar Ctrl+C ou fechar o script irá desligar todos os
% atuadores automaticamente por segurança.

clear; clc; close all;

% =========================================================================
%  CONFIGURAÇÃO DA PORTA SERIAL
% =========================================================================
COM_PORT = "COM9";   % Ajuste aqui a porta serial do seu Arduino R4 Central

% =========================================================================
%  INICIALIZAÇÃO DA PORTA SERIAL
% =========================================================================
try
    s = serialport(COM_PORT, 115200);
    configureTerminator(s, "LF");
    flush(s);
    fprintf('Conectado ao Arduino R4 na porta %s.\n\n', COM_PORT);
catch ME
    error('Não foi possível conectar à porta %s: %s', COM_PORT, ME.message);
end

% Configura o desligamento seguro automático (onCleanup)
cleanupObj = onCleanup(@() limparRecursos(s));

fprintf('=================================================================\n');
fprintf('             CALIBRAÇÃO DO SISTEMA DE HIDROGÊNIO (H2)            \n');
fprintf('=================================================================\n\n');

% =========================================================================
%  ETAPA 1: Medição do Nível Mínimo (H2 = 0%)
% =========================================================================
fprintf('--- ETAPA 1/4: NÍVEL MÍNIMO (H2 = 0%%) ---\n');
fprintf('1. Certifique-se de que o tanque de H2 está no nível mínimo de água.\n');
fprintf('   (Isto corresponde a 0%% de Hidrogênio).\n');
fprintf('Pressione ENTER quando estiver pronto para iniciar a medição...');
input('');

fprintf('Lendo nível estável (aguarde 2 segundos)...\n');
leituras_min = [];
for t = 1:20
    writeline(s, '0.00,0.00,0,0,0.00,0.00');
    data_str = readline(s);
    valores = str2double(split(data_str, ','));
    if length(valores) >= 1 && ~isnan(valores(1))
        leituras_min(end+1) = valores(1);
    end
    pause(0.1);
end

if isempty(leituras_min)
    error('Erro: Nenhuma leitura válida recebida do sensor. Verifique conexões.');
end

level_min = mean(leituras_min);
fprintf('Nível Mínimo Registrado: %.2f mm\n\n', level_min);

% =========================================================================
%  ETAPA 2: Teste do Eletrolisador (Enchimento - k3)
% =========================================================================
fprintf('--- ETAPA 2/4: ELETROLISADOR (Enchimento - k3) ---\n');
fprintf('O eletrolisador será LIGADO para encher o tanque de H2.\n');
fprintf('Pressione ENTER para ligar o eletrolisador e iniciar o teste...');
input('');

h_box = msgbox('O ELETROLISADOR ESTÁ LIGADO! Clique em OK quando o reservatório estiver cheio (H2 = 100%).', 'Calibração H2 - Eletrolisador');

t_start_el = tic;
level_history_el = [];
time_history_el = [];

while ishandle(h_box)
    writeline(s, '0.00,0.00,0,1,0.00,0.00');
    data_str = readline(s);
    valores = str2double(split(data_str, ','));
    
    if length(valores) >= 1 && ~isnan(valores(1))
        lvl = valores(1);
        level_history_el(end+1) = lvl;
        t_el = toc(t_start_el);
        time_history_el(end+1) = t_el;
        fprintf('Tempo: %5.1f s | Nível Atual: %5.2f mm | Var. Nível: %5.2f mm\n', t_el, lvl, lvl - level_min);
    end
    pause(0.1);
end

% Desliga o eletrolisador
writeline(s, '0.00,0.00,0,0,0.00,0.00');
t_fill = toc(t_start_el);

if isempty(level_history_el)
    error('Erro: Nenhuma leitura coletada durante o enchimento.');
end

level_max = level_history_el(end);
k3_medio = (level_max - level_min) / t_fill;

fprintf('\nEletrolisador desligado.\n');
fprintf('Tempo de Enchimento: %.1f s\n', t_fill);
fprintf('Nível Máximo Registrado: %.2f mm\n', level_max);
fprintf('Faixa Útil (Range): %.2f mm\n', level_max - level_min);
fprintf('Taxa de Enchimento média (k3): %.4f mm/s\n\n', k3_medio);

% =========================================================================
%  ETAPA 3: Teste de Perda Passiva (Vazamento)
% =========================================================================
fprintf('--- ETAPA 3/4: PERDAS PASSIVAS (Vazamento) ---\n');
fprintf('O sistema ficará DESLIGADO por 30 segundos para avaliar vazamento natural.\n');
fprintf('Iniciando o teste de perdas em 3 segundos...\n');
pause(3);

t_start_loss = tic;
level_history_loss = [];
time_history_loss = [];

while toc(t_start_loss) < 30.0
    writeline(s, '0.00,0.00,0,0,0.00,0.00');
    data_str = readline(s);
    valores = str2double(split(data_str, ','));
    
    if length(valores) >= 1 && ~isnan(valores(1))
        lvl = valores(1);
        level_history_loss(end+1) = lvl;
        t_loss = toc(t_start_loss);
        time_history_loss(end+1) = t_loss;
        fprintf('Tempo: %5.1f / 30.0 s | Nível Atual: %5.2f mm | Queda: %5.2f mm\n', t_loss, lvl, level_max - lvl);
    end
    pause(0.1);
end

t_loss_act = toc(t_start_loss);
level_after_loss = level_history_loss(end);
k_loss = (level_max - level_after_loss) / t_loss_act;

fprintf('\nFim do teste de perdas.\n');
fprintf('Queda de Nível Natural: %.2f mm em %.1f s\n', level_max - level_after_loss, t_loss_act);
fprintf('Taxa de Perda Passiva (k_loss): %.4f mm/s\n\n', k_loss);

% =========================================================================
%  ETAPA 4: Teste da Célula a Combustível (Consumo - k4)
% =========================================================================
fprintf('--- ETAPA 4/4: CÉLULA A COMBUSTÍVEL (Consumo - k4) ---\n');
fprintf('A célula a combustível será LIGADA para consumir o H2 e abaixar o nível.\n');
fprintf('Pressione ENTER para ligar a célula a combustível e iniciar o teste...');
input('');

h_box2 = msgbox('A CÉLULA A COMBUSTÍVEL ESTÁ LIGADA! Clique em OK quando o nível de água estiver próximo ao mínimo (H2 = 0%) ou o teste for concluído.', 'Calibração H2 - Célula');

t_start_fc = tic;
level_history_fc = [];
time_history_fc = [];

while ishandle(h_box2)
    writeline(s, '0.00,0.00,1,0,0.00,0.00');
    data_str = readline(s);
    valores = str2double(split(data_str, ','));
    
    if length(valores) >= 1 && ~isnan(valores(1))
        lvl = valores(1);
        level_history_fc(end+1) = lvl;
        t_fc = toc(t_start_fc);
        time_history_fc(end+1) = t_fc;
        fprintf('Tempo: %5.1f s | Nível Atual: %5.2f mm | Queda: %5.2f mm\n', t_fc, lvl, level_after_loss - lvl);
    end
    pause(0.1);
end

% Desliga tudo
writeline(s, '0.00,0.00,0,0,0.00,0.00');
t_fc_act = toc(t_start_fc);

if isempty(level_history_fc)
    error('Erro: Nenhuma leitura coletada durante o consumo.');
end

level_end_fc = level_history_fc(end);
k4_bruto = (level_after_loss - level_end_fc) / t_fc_act;
k4_medio = k4_bruto - k_loss;

% =========================================================================
%  CÁLCULO DAS TAXAS DINÂMICAS (VARIAÇÃO NO TEMPO)
% =========================================================================
% Usamos uma janela de 5 segundos para suavizar o ruído do sensor
fprintf('Calculando a variação temporal das taxas...\n');
window_size = 5; % Janela de regressão linear em segundos
Ts = 0.1;        % Período amostral (100 ms)

k3_dynamic = calcular_taxa_dinamica(time_history_el, level_history_el, window_size, Ts);
k4_dynamic = calcular_taxa_dinamica(time_history_fc, level_history_fc, window_size, Ts);
% Como k4 diminui o nível, multiplicamos por -1 para mostrar taxa positiva de consumo,
% e descontamos a perda passiva (k_loss)
k4_dynamic = -k4_dynamic - k_loss;

% =========================================================================
%  APRESENTAÇÃO E ANÁLISE DOS RESULTADOS
% =========================================================================
fprintf('=================================================================\n');
fprintf('                  RESULTADOS DA CALIBRAÇÃO                       \n');
fprintf('=================================================================\n');
fprintf('  Nível Mínimo (H2 = 0%%):    %6.2f mm\n', level_min);
fprintf('  Nível Máximo (H2 = 100%%):  %6.2f mm\n', level_max);
fprintf('  Faixa Útil (Capacidade):   %6.2f mm\n', level_max - level_min);
fprintf('  Taxa Eletrolisador (k3):   %8.6f mm/s\n', k3_medio);
fprintf('  Taxa Perda Passiva:        %8.6f mm/s\n', k_loss);
fprintf('  Taxa Célula (k4):          %8.6f mm/s\n', k4_medio);
fprintf('-----------------------------------------------------------------\n');
fprintf('  Para atualizar seus scripts de controle:\n');
fprintf('\n  >> No arquivo: microgrid_init_w_demand.m <<\n');
fprintf('     Copie e substitua as taxas nas linhas 34 e 35:\n\n');
fprintf('     k3 = %.6f * Ts;  %% Taxa de enchimento medida\n', k3_medio);
fprintf('     k4 = %.6f * Ts;  %% Taxa de consumo medida\n', k4_medio);
fprintf('\n  >> No arquivo: firmware_painel_solar / web_app / etc. <<\n');
fprintf('     Seu tanque de água opera de %.2f mm a %.2f mm.\n', level_min, level_max);
fprintf('=================================================================\n');

% Plota as curvas de nível e as taxas dinâmicas
fig = figure('Name', 'Curvas e Taxas Dinâmicas H2', 'NumberTitle', 'off');
tiledlayout(fig, 3, 2);

% --- Subplot 1: Nível Enchimento ---
ax1 = nexttile;
plot(ax1, time_history_el, level_history_el, 'b', 'LineWidth', 2);
grid(ax1, 'on');
title(ax1, 'Nível de Água (Enchimento)');
xlabel(ax1, 'Tempo (s)');
ylabel(ax1, 'Nível (mm)');

% --- Subplot 2: Taxa Dinâmica k3 ---
ax2 = nexttile;
plot(ax2, time_history_el, k3_dynamic * 60, 'Color', [0 0.5 0], 'LineWidth', 2);
grid(ax2, 'on');
title(ax2, 'Taxa de Geração Dinâmica k3');
xlabel(ax2, 'Tempo (s)');
ylabel(ax2, 'Taxa (mm/min)');
legend(ax2, 'k3 (mm/min)');

% --- Subplot 3: Nível Perdas ---
ax3 = nexttile;
plot(ax3, time_history_loss, level_history_loss, 'k', 'LineWidth', 2);
grid(ax3, 'on');
title(ax3, 'Nível de Água (Perda Passiva)');
xlabel(ax3, 'Tempo (s)');
ylabel(ax3, 'Nível (mm)');

% --- Subplot 4: Espaço vazio para alinhar gráficos ---
nexttile;
axis('off');

% --- Subplot 5: Nível Esvaziamento ---
ax5 = nexttile;
plot(ax5, time_history_fc, level_history_fc, 'r', 'LineWidth', 2);
grid(ax5, 'on');
title(ax5, 'Nível de Água (Esvaziamento)');
xlabel(ax5, 'Tempo (s)');
ylabel(ax5, 'Nível (mm)');

% --- Subplot 6: Taxa Dinâmica k4 ---
ax6 = nexttile;
plot(ax6, time_history_fc, k4_dynamic * 60, 'Color', [0.8 0 0], 'LineWidth', 2);
grid(ax6, 'on');
title(ax6, 'Taxa de Consumo Dinâmica k4');
xlabel(ax6, 'Tempo (s)');
ylabel(ax6, 'Taxa (mm/min)');
legend(ax6, 'k4 (mm/min)');

% =========================================================================
%  FUNÇÕES AUXILIARES
% =========================================================================

function limparRecursos(s)
    % Executado automaticamente se o script for interrompido ou terminar
    try
        fprintf('\nDesligando atuadores por segurança...\n');
        % Envia tudo 0
        writeline(s, '0.00,0.00,0,0,0.00,0.00');
        pause(0.2);
    catch
    end
    try
        clear s;
        fprintf('Porta serial do Arduino fechada.\n');
    catch
    end
end

function k_dynamic = calcular_taxa_dinamica(time_hist, level_hist, window_seconds, Ts)
    % Calcula a taxa instantânea usando regressão linear móvel (sliding window)
    N = length(level_hist);
    k_dynamic = zeros(1, N);
    window_size = round(window_seconds / Ts);
    
    for idx = 1:N
        start_idx = max(1, idx - window_size);
        if idx - start_idx >= 5 % pelo menos 5 pontos para a reta
            p = polyfit(time_hist(start_idx:idx), level_hist(start_idx:idx), 1);
            k_dynamic(idx) = p(1); % inclinação em mm/s
        else
            if idx > 1
                k_dynamic(idx) = k_dynamic(idx-1); % mantém último valor estimado
            else
                k_dynamic(idx) = 0;
            end
        end
    end
end
