%% Parameters

theta_f = 20;
k = 300;
M = 5;
xf = 0.3947;
xc = 0.3816;
alpha = 0.117;

Ts = 0.1;

c0 = 0.2632;
v0 = 0.6519;

x_prev = [c0;v0];

%% Create MPC object

N = 15;
N_h_ctr = 5;

mpc = init_mpc(N,N_h_ctr);
%% LTI system

A = [-1/theta_f-k*exp(-M/v0) -k*c0*M*exp(-M/v0)/(v0^2);
     k*exp(-M/v0) -1/theta_f];

B = [0; -alpha*(v0-xc)];

Bd = [1/theta_f k*c0*M*exp(-M/v0)/(v0^2); xf/theta_f 0];

C = [1 0];
C = eye(2);

sys_ct = ss(A,B,C,0);
sys = c2d(sys_ct,Ts);

mpc = init_mpc_system(mpc,eye(2)+Ts*A,Ts*B,Ts*Bd,C,[0;0],[]);

%% Pre-computing Aeq in a hash table

% LPV matrices
A0 = eye(2) + Ts*[-1/theta_f 0; 
                    0, -1/theta_f];
A1 = Ts*[-k 0; 
          k 0];
A2 = Ts*[0 -k*M;
         0 0];
A3 = [0 0; 0 0];


B0 = Ts*[0; alpha*xc];
B1 = [0; 0];
B2 = [0; 0];
B3 = Ts*[0; -alpha];

Bd0 = Ts*[1/theta_f 0;
           xf/theta_f 0];
Bd1 = [0 0; 0 0];
Bd2 = Ts*[0 k*M; 0 0];
Bd3 = [0 0; 0 0];

n_rho = 3;

[mpc.Aeq0, mpc.Aeq1, mpc.Aeq2, mpc.Aeq3] = init_mpc_Aeq(mpc, A0, B0, n_rho, A1, A2, A3, B1, B2, B3);
[mpc.beq0, mpc.beq1, mpc.beq2, mpc.beq3, mpc.beq_ff0, mpc.beq_ff1, mpc.beq_ff2, mpc.beq_ff3] = init_mpc_beq(mpc, A0, Bd0, n_rho, ...
                                                                                                A1, A2, A3, Bd1, Bd2, Bd3);

%% Constraints

x_min = [0;0];
x_max = [1;1];
tightening = 0.1;
mpc = init_mpc_state_cnstr(mpc,x_min,x_max,tightening);

x_ter_min = [];%;0.05*ones(nx,1);
x_ter_max = [];%1*ones(nx,1);
%mpc = init_mpc_ter_state_cnstr(mpc,x_ter_min,x_ter_max);

u_min = 0;
u_max = 1;
mpc = init_mpc_u_cnstr(mpc,u_min,u_max);

du_min = -0.1*ones(mpc.nu,1);
du_max = 0.1*ones(mpc.nu,1);
mpc = init_mpc_delta_u_cnstr(mpc,du_min,du_max);

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

Qx = diag([3000 0.1]);
Ru = 1;
ter_constraint = 0;
x_ref_is_y = 1;

P = 1e4*[3.7 -1.2; -1.2 1];

%[mpc] = init_mpc_ter_ingredients_dlqr(mpc,Qx,Ru,ter_constraint,x_ref_is_y, P);

%% Costs

Qe = diag([5000 250]);
mpc = init_mpc_Tracking_cost(mpc,Qe);

Rdu = 1;
%mpc = init_mpc_DiffControl_cost(mpc,Rdu);

Ru = 10;
%mpc = init_mpc_Control_cost(mpc,Ru);

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

x0 = 0.45*ones(mpc.Nu+mpc.Nx,1);
u_prev = 0.45;
d = [1;v0];
x0 = init_mpc_warm_start(mpc,x_prev,u_prev,d);
