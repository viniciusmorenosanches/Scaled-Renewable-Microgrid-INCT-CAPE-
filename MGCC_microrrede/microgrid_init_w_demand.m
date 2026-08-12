% microgrid_init_w_demand.m
% Inicialização do MPC para o cenário de 24h com demanda variável.
% Chamado por MGCC_w_demand.m no início da execução.
%
% Saídas relevantes para MGCC_w_demand.m:
%   mpc    — objeto com toda a configuração do solver LPV-PILOT
%   x0     — condição inicial do otimizador (warm-start)
%   u_prev — controle inicial [Pc; Pd; uel; ucc] normalizados
%   Bd0    — matriz de perturbações (usada no update a cada passo)
%   n_rho  — número de parâmetros LPV (0 = LTI)

% clear all
% close all
% clc

%% -------------------------------------------------------------------------
%  Parâmetros do modelo de estado discreto (Ts = 100 ms)
%  Estado: x = [SoC; nível_H2 (normalizado /10); Pot_total (normalizado /1000)]
%  Entrada: u = [Pc; Pd; uel; ucc]  — todas normalizadas entre 0 e 1
%  Perturbação: d = [Pot_wt; Pot_pv]  — em W
% -------------------------------------------------------------------------

Ts = 0.1;   % período de amostragem [s]

% Coeficientes do modelo de bateria (SoC)
%   SoC[k+1] = delta_soc * SoC[k] + k1 * Pc[k] - k2 * Pd[k]
%   k1 = eta_carga  * (Ts/3600) / E_nom     (normalizado por 400 W)
%   k2 = eta_descarga * (Ts/3600) / E_nom
k1 = 3.92e-5 * 400;   % coef. de carga da bateria
k2 = 4.08e-5 * 400;   % coef. de descarga da bateria

% Coeficientes do modelo de nível de H2 (em mm, normalizado /10)
%   nH2[k+1] = nH2[k] + k3 * uel[k] - k4 * ucc[k]
k3 = 0.1  * Ts;    % taxa de enchimento pelo eletrolisador [mm/s]
k4 = 0.0658 * Ts;  % taxa de esvaziamento pela célula combustível [mm/s]

% Ganhos dos atuadores H2 (para cálculo de Pot_total)
%   Pot_cc = kcc * ucc  [W]   — potência gerada pela célula combustível
%   Pot_el = kel * uel  [W]   — potência consumida pelo eletrolisador
kcc = 154.57;  % [W] — medido experimentalmente
kel = 10;      % [W] — medido experimentalmente

% Normalizações para o modelo (escala MPC trabalha em [0,1])
k5 = kcc / 1000;
k6 = kel / 1000;
k7 = 1 / 1000;
k8 = 1 / 1000;

% Fatores de auto-decaimento
delta_soc = 0.995;   % perda passiva de SoC por passo
delta_nh2 = 1;       % nível de H2 sem perdas passivas

% Condições iniciais
soc0  = 0.6;   % SoC inicial [0..1]
nh20  = 5;     % nível de H2 inicial [mm]
pot0  = 0.2;   % Pot_total inicial normalizada

x_prev = [soc0; nh20 / 10; pot0];

%% -------------------------------------------------------------------------
%  Criação do objeto MPC (solver LPV-PILOT)
% -------------------------------------------------------------------------

N       = 15;   % horizonte de predição
N_h_ctr = 15;   % horizonte de controle

mpc = init_mpc(N, N_h_ctr);
mpc.linear_solver = 3;

%% Modelo LTI discreto
%
%  x[k+1] = A*x[k] + B*u[k] + Bd*d[k]
%
%  x = [SoC; nH2/10; Pot_total/1000]     (3 estados)
%  u = [Pc; Pd; uel; ucc] / 400          (4 entradas normalizadas 0..1)
%  d = [Pot_wt; Pot_pv]                  (2 perturbações, em W)

A = [delta_soc,  0,         0;
     0,          delta_nh2, 0;
     0,          0,         0];

B = [k1,        -k2,       0,         0;
     0,           0,       k3 / 10,  -k4 / 10;
     -400/1000, 400/1000, -k6,        k5];

Bd = [0,       0;
      0,       0;
      1/1000,  1/1000];

C = eye(3);

mpc = init_mpc_system(mpc, A, B, Bd, C, [0; 0; 0], []);

%% -------------------------------------------------------------------------
%  Pré-computação das matrizes de igualdade (Aeq, beq)
%  n_rho = 0 indica sistema LTI (sem parametrização LPV no momento)
% -------------------------------------------------------------------------

A0  = A;
B0  = B;
Bd0 = Bd;
n_rho = 0;

[mpc.Aeq0]           = init_mpc_Aeq(mpc, A0, B0, n_rho);
[mpc.beq0, mpc.beq_ff0] = init_mpc_beq(mpc, A0, Bd0, n_rho);

%% -------------------------------------------------------------------------
%  Restrições de estado e entrada
% -------------------------------------------------------------------------

% Estados: [SoC ∈ [-0.02, 1.02]; nH2/10 ∈ [-0.02, 10.02]; Pot_total/1000 ∈ [-0.05, 1.05]]
x_min = [-0.02; -0.02; -0.05];
x_max = [1.02; 10.02; 1.05];
mpc = init_mpc_state_cnstr(mpc, x_min, x_max, 0);

% Entradas normalizadas: todas entre 0 e 1
u_min = [0; 0; 0; 0];
u_max = [1; 1; 1; 1];
mpc = init_mpc_u_cnstr(mpc, u_min, u_max);

%% -------------------------------------------------------------------------
%  Custo do MPC
%  Tracking: penaliza desvio do estado de referência
%  Control:  penaliza esforço de controle (Pc, Pd mais caros que H2)
% -------------------------------------------------------------------------

% Peso do erro de rastreamento por estado
%   [SoC, nH2, Pot_total] — Pot_total tem peso 100x maior (equilíbrio de energia)
Qe = diag([1, 1, 100]);
mpc = init_mpc_Tracking_cost(mpc, Qe);

% Peso do esforço de controle por entrada
%   [Pc, Pd, uel, ucc] — uel e ucc tem custo 0.001 (mais baratos que a bateria)
Ru = diag([0.01, 0.01, 0.001, 0.001]);
mpc = init_mpc_Control_cost(mpc, Ru);

%% -------------------------------------------------------------------------
%  Configuração do solver
% -------------------------------------------------------------------------

mpc.t = 500;   % parâmetro de barreira logarítmica (agressividade do solver)

%% -------------------------------------------------------------------------
%  Condições iniciais do otimizador (warm-start)
% -------------------------------------------------------------------------

u_prev = [0; 0; 0; 0];
d      = [100; 100];   % perturbação inicial (Pot_wt, Pot_pv em W)
x0     = init_mpc_warm_start(mpc, x_prev, u_prev, d);
