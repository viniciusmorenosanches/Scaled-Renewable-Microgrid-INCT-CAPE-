function plotar_resultados_mgcc(arquivo_mat, SoC_ref, ref_h2, N_pts)
% plotar_resultados_mgcc — Gera os gráficos finais após uma simulação do MGCC.
%
% Uso:
%   plotar_resultados_mgcc('dados_microrrede_test.mat', 0.6, 12, 2400)
%
% Parâmetros:
%   arquivo_mat — caminho para o arquivo .mat salvo pelo MGCC_w_demand.m
%   SoC_ref     — referência de SoC usada na simulação (para plotar linha pontilhada)
%   ref_h2      — referência de nível de H2 [mm]
%   N_pts       — número de pontos válidos (útil quando a simulação parou antes do fim)

close all

dat   = load(arquivo_mat);
N_pts = min(N_pts, length(dat.SoC));

if N_pts < 2
    disp("Poucos dados para gerar gráficos.");
    return;
end

% Eixo de tempo em horas simuladas (2400 passos = 24h)
time = (0:N_pts-1) * (24 / 2400);

% Ajusta tamanho das variáveis para paradas prematuras da simulação
dat.nivelAgua = dat.nivelAgua(1:N_pts);
dat.SoC = dat.SoC(1:N_pts);
dat.dat_pot_carga = dat.dat_pot_carga(1:N_pts);
dat.dat_pot_descarga = dat.dat_pot_descarga(1:N_pts);
dat.Pot_el = dat.Pot_el(1:N_pts);
dat.Pot_cc = dat.Pot_cc(1:N_pts);

% Remover zeros espúrios (causados por perda de pacote serial)
campos = {'Pot_total', 'Pot_wt', 'Pot_pv', 'demand'};
for c = campos
    arr = dat.(c{1})(1:N_pts);
    for k = 2:N_pts
        if arr(k) == 0
            arr(k) = arr(k-1);
        end
    end
    dat.(c{1})(1:N_pts) = arr;
end

% Suavizar Pot_pv (muito ruidoso) com média móvel de 10 s
dat.Pot_pv(1:N_pts) = movmean(dat.Pot_pv(1:N_pts), 100);

% Garantir mínimo de Pot_total e Pot_wt (evita zeros físicos irreais)
dat.Pot_total(1:N_pts) = max(50, dat.Pot_total(1:N_pts));
for k = 2:N_pts
    if dat.Pot_wt(k) < 50
        dat.Pot_wt(k) = dat.Pot_wt(k-1);
    end
end

idx = 1:2:N_pts;   % subsamplear 2x para plotagem mais rápida

%% Figura 1 — Visão geral do sistema
figure('Name', 'Resultados MGCC — Visão Geral', 'NumberTitle', 'off');

subplot(4, 1, 1)
plot(time(idx), dat.Pot_total(idx), 'LineWidth', 1.5); hold on
plot(time(idx), dat.demand(idx),         'LineWidth', 1.5)
legend('P_{total}(k)', 'P_{demand}(k)', 'FontSize', 12)
ylabel('Potência (W)', 'FontSize', 13)
title('Balanço de potência vs demanda')
xlim([0, 24]); grid on

subplot(4, 1, 2)
plot(time(idx), dat.Pot_wt(idx), 'LineWidth', 2.0); hold on
plot(time(idx), dat.Pot_pv(idx), 'LineWidth', 2.0)
legend('P_{wt}(k)', 'P_{pv}(k)', 'FontSize', 12)
ylabel('Renováveis (W)', 'FontSize', 13)
title('Geração solar e eólica')
xlim([0, 24]); grid on

subplot(4, 1, 3)
plot(time(idx), dat.SoC(idx), 'LineWidth', 2.0); hold on
plot(time(idx), SoC_ref * ones(size(idx)), 'r--', 'LineWidth', 2.0)
legend('SoC(k)', 'SoC_{ref}', 'FontSize', 12)
ylabel('SoC', 'FontSize', 13)
title('Estado de carga da bateria')
xlim([0, 24]); ylim([0, 1]); grid on

subplot(4, 1, 4)
plot(time(idx), dat.nivelAgua(idx), 'LineWidth', 2.0); hold on
plot(time(idx), ref_h2 * ones(size(idx)), 'r--', 'LineWidth', 2.0)
legend('H_2(k)', 'H_2^{ref}', 'FontSize', 12)
ylabel('Nível H_2 (mm)', 'FontSize', 13)
title('Nível de hidrogênio no reservatório')
xlabel('Tempo (h)', 'FontSize', 13)
xlim([0, 24]); grid on

%% Figura 2 — Ações de controle
figure('Name', 'Resultados MGCC — Controles', 'NumberTitle', 'off');

subplot(2, 1, 1)
plot(time, -dat.dat_pot_carga + dat.dat_pot_descarga, 'LineWidth', 2.0)
legend('P_{desc} (>0) / P_{carga} (<0)', 'FontSize', 12)
ylabel('Bateria (W)', 'FontSize', 13)
title('Comando de carga/descarga da bateria')
xlim([0, 24]); grid on

subplot(2, 1, 2)
Pot_el_norm = min(1, dat.Pot_el(1:N_pts));
Pot_cc_norm = min(1, dat.Pot_cc(1:N_pts));
stairs(time, -Pot_el_norm + Pot_cc_norm, 'LineWidth', 2.0)
legend('u_{cc} (>0) / u_{el} (<0)', 'FontSize', 12)
ylabel('Elecrolisador / Célula', 'FontSize', 13)
title('Comando dos dispositivos de H_2')
xlabel('Tempo (h)', 'FontSize', 13)
xlim([0, 24]); grid on

end
