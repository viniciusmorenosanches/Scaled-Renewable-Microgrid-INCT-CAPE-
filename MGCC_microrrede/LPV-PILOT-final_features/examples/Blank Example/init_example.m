%% Parameters

% Here you put the parameters of your model
alpha = ...;
beta = ...;
k = ...;
g = 9.81;   % m/s^2

% Sampling time
Ts = ...;   % s

% Initial states
x1 = ...;
x2 = ...;
xn = ...;

x_prev = [x1;x2; ...; xn];

%% Create MPC object

% choose Np = Nu for stability assurance
Np = ...;   % Prediction horizon 
Nu = ...;   % Control horizon

mpc = init_mpc(Np, Nu);

%% LTI system
% x' = A x(t) + B u(t) + Bd d(t)
% y(t) = C x(t)
% x(t): states
% u(t): inputs
% d(t): disturbance

A = [...];

B = [...];

Bd = [...];

C = eye(2);

% Continuous system
sys_ct = ss(A,B,C,0);

% Conversion to discrete system
sys_d = c2d(sys_ct,Ts);

mpc = init_mpc_system(mpc, sys_d.A, sys_d.B, Ts*Bd, C,[0;0],[]);
%% Constraints

% States
x_min = [...; ...];
x_max = [...; ...];
tightening = ...;   % 0 <= tightening <= 1 represents the constraint tightening
% to disable constraint tightening use tightening = [] (not recommended)

mpc = init_mpc_state_cnstr(mpc,x_min,x_max,tightening);

% Terminal (not mandatory)
x_ter_min = []; %Example: 0.05*ones(nx,1);
x_ter_max = []; %Example: 1*ones(nx,1);
%mpc = init_mpc_ter_state_cnstr(mpc,x_ter_min,x_ter_max);

% Control signal
u_min = ...; % if more than one input use [...;...;...]
u_max = ...;
mpc = init_mpc_u_cnstr(mpc,u_min,u_max);

% Control variation (not mandatory)
du_min = ...; % Example: -0.1*ones(mpc.nu,1);
du_max = ...; % Example: 0.1*ones(mpc.nu,1);
mpc = init_mpc_delta_u_cnstr(mpc,du_min,du_max);

% Output (not mandatory)
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

Qx = diag([... ...]); % for state x1 and x2
Ru = ...; % for control input

ter_constraint = 0;
x_ref_is_y = 1;

P = [... ...; ... ...]; % Optional: P matrix (for stabilizing law)

%[mpc] = init_mpc_ter_ingredients_dlqr(mpc,Qx,Ru,ter_constraint,x_ref_is_y, P);

%% Costs

Qe = diag([... ...]);   % Cost of x1, x2, ..., xn
mpc = init_mpc_Tracking_cost(mpc,Qe);

Rdu = ...;  % Cost of control variation
%mpc = init_mpc_DiffControl_cost(mpc,Rdu);

Ru = ...;   % Cost of control signal
%mpc = init_mpc_Control_cost(mpc,Ru);

%% Performance Cost Matrix

Cz = [];
Dz = [];
Ddz = [];

Qz = [];

%mpc = init_mpc_LinPerf_cost(mpc,Qz,Cz,Dz,Ddz);
%% Set QP solver to be more aggressive

% Fixed value in 500. You can adapt to your specific case
mpc.t = 500;

%% Init conditions for simulation

x0 = ...; % Initial state. Example: 0.45*ones(mpc.Nu+mpc.Nx,1);
u_prev = ...;   % Previous control input
d = [...; ...]; % Disturbance
x0 = init_mpc_warm_start(mpc,x_prev,u_prev,d);
