clear all
close all
clc

%% Parameters

Ts = 0.1;

k1 = 3.92e-5 * 400;
k2 = 4.08e-5 * 400;
%k3 = 130e-9;
k3 = 0.1 * Ts; % mm/s
%k4 = 82.63e-9;
k4 = 0.0658 * Ts; % mm/s

kcc = 154.57;
kel = 10;
kel = 150;    % É 10!!

k5 = kcc/1000;
k6 = kel/1000;
k7 = 1/1000;
k8 = 1/1000;

delta_soc = 0.995;
delta_nh2 = 1;

soc0 = 0.6;
nh20 = 5;
pot0 = 0.2;

x_prev = [soc0;nh20;pot0];

%% Create MPC object

N = 15;
N_h_ctr = 15;

mpc = init_mpc(N,N_h_ctr);
%% LTI system

A = [delta_soc, 0, 0;
     0 delta_nh2, 0;
     0, 0, 0];

B = [k1, -k2, 0, 0;
     0, 0, k3, -k4;
     -400/1000, 400/1000, -k6, k5];

Bd = [0, 0; 0, 0; 1/1000, 1/1000];

C = eye(3);

%sys_ct = ss(A,B,C,0);
%sys = c2d(sys_ct,Ts);

mpc = init_mpc_system(mpc,A,B,Bd,C,[0;0;0],[]);

%% Pre-computing Aeq in a hash table

% LPV matrices
A0 = A;

B0 = B;

Bd0 = Bd;

n_rho = 0;

[mpc.Aeq0] = init_mpc_Aeq(mpc, A0, B0, n_rho);
[mpc.beq0, mpc.beq_ff0] = init_mpc_beq(mpc, A0, Bd0, n_rho);

%% Constraints

x_min = [0;0;0];
x_max = [1;10;1];
tightening = 0;
mpc = init_mpc_state_cnstr(mpc,x_min,x_max,tightening);

x_ter_min = [];%;0.05*ones(nx,1);
x_ter_max = [];%1*ones(nx,1);
%mpc = init_mpc_ter_state_cnstr(mpc,x_ter_min,x_ter_max);

u_min = [0;0;0;0];
u_max = [1;1;1;1];
mpc = init_mpc_u_cnstr(mpc,u_min,u_max);

%du_min = -0.1*ones(mpc.nu,1);
%du_max = 0.1*ones(mpc.nu,1);
%mpc = init_mpc_delta_u_cnstr(mpc,du_min,du_max);

y_min = [];
y_max = [];
%mpc = init_mpc_output_cnstr(mpc,y_min,y_max);

%% General Linear Inequalities

Ci = [];
Di = [];
Ddi = [];

yi_min = [];
yi_max = [];

%mpc = init_mpc_general_lin_ineq_cnstr(mpc,yi_min,yi_max,Ci,Di,Ddi);

%% Terminal Ingredients

% Qx = diag([3000 0.1]);
% Ru = 1;
% ter_constraint = 0;
% x_ref_is_y = 1;
% 
% P = 1e4*[3.7 -1.2; -1.2 1];

%[mpc] = init_mpc_ter_ingredients_dlqr(mpc,Qx,Ru,ter_constraint,x_ref_is_y, P);

%% Costs

Qe = diag([1, 1, 100]);
mpc = init_mpc_Tracking_cost(mpc,Qe);

%Rdu = 1;
%mpc = init_mpc_DiffControl_cost(mpc,Rdu);

Ru = diag([0.01, 0.01, 0.01, 0.001]);
%Ru = 0.01;
mpc = init_mpc_Control_cost(mpc,Ru);

%% Performance Cost Matrix

Cz = [];
Dz = [];
Ddz = [];

Qz = [];

%mpc = init_mpc_LinPerf_cost(mpc,Qz,Cz,Dz,Ddz);
%% Set QP solver to be more aggressive

mpc.t = 500;

%%
%mpc = defLtiMpc(N,A,B,C,D,Bd,Dd,Qe,Rdu,Ru,Cz,Dz,Ddz,Qz,Ci,Di,Ddi,...
%    x_min,x_max,x_ter_min,x_ter_max,u_min,u_max,du_min,du_max,y_min,y_max,yi_min,yi_max)

%% Init conditions for simulation

x0 = 0*ones(mpc.Nu+mpc.Nx,1);
u_prev = [0;0;0;0];
d = [100;100];
x0 = init_mpc_warm_start(mpc,x_prev,u_prev,d);

%% Unit test
        
% Fixed demand
% demand = 200;
% 
% xref = [0.6;5e-3;demand];
% 
% x_prev = [0.0;0*10^(-3);0];
% 
% d = [150; 150];
% 
% tic
% [u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);
% toc
