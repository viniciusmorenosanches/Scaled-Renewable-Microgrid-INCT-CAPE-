close all

warning('off')


%% Parameters

M = 1;    % mass of the cart [kg]
m = 0.1;  % mass of the ball [kg]
l = 0.8;  % length of the rod [m]
g = 9.81; % gravity constant [m/s^2]

Ts = 0.1;

% Initial condition
p0 = -5;
theta0 = pi/6;
v0 = 0;
omega0 = -1;

x_prev = [p0; theta0; v0; omega0];

%% Create MPC object

N = 50;
N_h_ctr = N;

state_cost = [2.5, 10, 0.01, 0.01];
input_cost = 1;
inc_input_cost = 0;

mpc = init_mpc(N,N_h_ctr);
%% LTI system

rho_1 = ( -m*l*sin(theta0)*omega0 )/(M + m - m*cos(theta0)^2);
rho_3 = -m*cos(theta0)*sin(theta0)*omega0 / (M + m - m*cos(theta0)^2);
rho_5 = 1/(M + m - m*cos(theta0)^2);
rho_6 = cos(theta0)/( l * (M + m - m*cos(theta0)^2) );

if abs(theta0 - 0) < 1e-6
    rho_2 = 0;
    rho_4 = 0;
else
    rho_2 = m*g*cos(theta0)*sin(theta0) / ( (M + m - m*cos(theta0)^2) * theta0 );
    rho_4 = (M + m)*g*sin(theta0) / ( (M + m - m*cos(theta0)^2) * (l*theta0) );
end

A = [0 0 1 0;
     0 0 0 1;
     0 rho_2 0 rho_1;
     0 rho_4 0 rho_3];

B = [0; 0; rho_5; rho_6];

Bd = [0; 0; 0; 0];

%C = [];
C = eye(4);

mpc = init_mpc_system(mpc,eye(4)+Ts*A,Ts*B,Ts*Bd,C,[0;0],[]);

%% Pre-computing Aeq

% LPV matrices
A0 = eye(4) + Ts*[0 0 1 0;
                  0 0 0 1;
                  0 0 0 0;
                  0 0 0 0];
A1 = Ts*[0 0 0 0;
          0 0 0 0;
          0 0 0 1;
          0 0 0 0];
A2 = Ts*[0 0 0 0;
          0 0 0 0;
          0 1 0 0;
          0 0 0 0];
A3 = Ts*[0 0 0 0;
          0 0 0 0;
          0 0 0 0;
          0 0 0 1];
A4 = Ts*[0 0 0 0;
          0 0 0 0;
          0 0 0 0;
          0 1 0 0];
A5 = zeros(4);
A6 = zeros(4);

B0 = Ts*[0; 0; 0; 0];
B1 = Ts*[0; 0; 0; 0];
B2 = Ts*[0; 0; 0; 0];
B3 = Ts*[0; 0; 0; 0];
B4 = Ts*[0; 0; 0; 0];
B5 = Ts*[0; 0; 1; 0];
B6 = Ts*[0; 0; 0; 1];

Bd0 = Ts*[0; 0; 0; 0];
Bd1 = Ts*[0; 0; 0; 0];
Bd2 = Ts*[0; 0; 0; 0];
Bd3 = Ts*[0; 0; 0; 0];
Bd4 = Ts*[0; 0; 0; 0];
Bd5 = Ts*[0; 0; 0; 0];
Bd6 = Ts*[0; 0; 0; 0];

n_rho = 6;

[mpc.Aeq0, mpc.Aeq1, mpc.Aeq2, mpc.Aeq3, mpc.Aeq4, mpc.Aeq5, mpc.Aeq6] = init_mpc_Aeq(mpc, A0, B0, n_rho, ...
                                                                                        A1, A2, A3, A4, A5, A6, ...
                                                                                        B1, B2, B3, B4, B5, B6);
[mpc.beq0, mpc.beq1, mpc.beq2, mpc.beq3, mpc.beq4, mpc.beq5, mpc.beq6, mpc.beq_ff0, mpc.beq_ff1, mpc.beq_ff2, mpc.beq_ff3, mpc.beq_ff4, mpc.beq_ff5, mpc.beq_ff6] = init_mpc_beq(mpc, A0, Bd0, n_rho, ...
                                                                                                A1, A2, A3, A4, A5, A6, ...
                                                                                                Bd1, Bd2, Bd3, Bd4, Bd5, Bd6);

%% Constraints
x_min = [-15;-pi; -10; -15];
x_max = [ 15; pi;  10;  15];

tightening = 0.45;   % 0 <= tightening <= 1 represents the constraint tightening
% to disable constraint tightening use tightening = [] (not recommended)

mpc = init_mpc_state_cnstr(mpc,x_min,x_max,tightening);

u_min = -20;
u_max = 20;
mpc = init_mpc_u_cnstr(mpc,u_min,u_max);

% du_min = -0.1*ones(mpc.nu,1);
% du_max = 0.1*ones(mpc.nu,1);
% mpc = init_mpc_delta_u_cnstr(mpc,du_min,du_max);

% y_min = [];
% y_max = [];
%mpc = init_mpc_output_cnstr(mpc,y_min,y_max);

%% General Linear Inequalities

Ci = [];
Di = [];
Ddi = [];

yi_min = [];
yi_max = [];

%mpc = init_mpc_general_lin_ineq_cnstr(mpc,yi_min,yi_max,Ci,Di,Ddi);

%% Terminal Ingredients

% Terminal constraint
x_ter_min = [0; 0; 0; 0];   % 0.05*ones(nx,1);
x_ter_min = [];
x_ter_max = [1; 1; 1; 1];       % 1*ones(nx,1);
mpc = init_mpc_ter_state_cnstr(mpc,x_ter_min,x_ter_max);

% Terminal cost
Qx = diag(state_cost);
Ru = input_cost;
ter_constraint = 1;
x_ref_is_y = 1;

den = M + m;

% System's matrices linearized around equilibrium point: theta = 0
A_d = eye(4) + Ts*[ 0,     0,      1,                        0;
                  0,     0,      0,                        1;
                  0, m*g/den,    0,                        0;
                  0, g*(M + m)/(l*den), 0,                0 ];

B_d = Ts*[ 0;
          0;
          1/den;
          1/(l*den) ];

Q_dlqr = diag(state_cost);
R_dlqr = input_cost; 

[K, P, ~] = dlqr(A_d, B_d, Q_dlqr, R_dlqr);

[mpc] = init_mpc_ter_ingredients_dlqr(mpc,Qx,Ru,ter_constraint,x_ref_is_y, P);

%% Costs

Qe = diag(state_cost);
mpc = init_mpc_Tracking_cost(mpc,Qe);

Ru = input_cost;
mpc = init_mpc_Control_cost(mpc,Ru);

Rdu = inc_input_cost;
mpc = init_mpc_DiffControl_cost(mpc,Rdu);

%% Just for Cost Calculus

Qe_vec = repmat(state_cost,1, mpc.N);
Qe_cost = diag(Qe_vec);

Ru_vec = repmat(input_cost, 1, mpc.N_ctr_hor);
Ru_cost = diag(Ru_vec);

%% Performance Cost Matrix

Cz = [];
Dz = [];
Ddz = [];

Qz = [];

%mpc = init_mpc_LinPerf_cost(mpc,Qz,Cz,Dz,Ddz);
%% Set QP solver to be more aggressive

mpc.t = 5;

%%
%mpc = defLtiMpc(N,A,B,C,D,Bd,Dd,Qe,Rdu,Ru,Cz,Dz,Ddz,Qz,Ci,Di,Ddi,...
%    x_min,x_max,x_ter_min,x_ter_max,u_min,u_max,du_min,du_max,y_min,y_max,yi_min,yi_max)

%% Init conditions for simulation

%x0 = 0.0*ones(mpc.Nu+mpc.Nx,1);
u_prev = 0;
d = [0];
x_ref = [0; 0; 0; 0];
x0 = init_mpc_warm_start(mpc,x_prev,u_prev,d,x_ref);
