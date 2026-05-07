clear all
close all
clc

import casadi.*

%% System parameters
gravity = 9.81;
l = 0.8;
m = 0.1;
M = 1.0;
dt = 0.1;

% MPC Parameters
N = 50;
Nu = N;
state_cost = [2.5, 10, 0.01, 0.01];
input_cost = 1;
inc_input_cost = 0;

% Computed using dlqr for linearized system in theta = 0;
P = [38.1353  -94.9361   27.1293  -28.6795;
      -94.9361  535.0348 -106.6437  153.4555;
       27.1293 -106.6437   28.0252  -32.3639;
      -28.6795  153.4555  -32.3639   45.5682];

%% State and Input variables
x = SX.sym('x');
theta = SX.sym('theta');
v = SX.sym('v');
omega = SX.sym('omega');
states = [x; theta; v; omega];
n_states = length(states);

u = SX.sym('u');
controls = u;
n_controls = length(controls);

%% Continuous dynamic
sin_theta = sin(theta);
cos_theta = cos(theta);
denom = M + m - m*cos_theta^2;

xdot = [v;
        omega;
        (-m*l*sin_theta*omega^2 + m*gravity*cos_theta*sin_theta + u)/denom;
        (-m*l*cos_theta*sin_theta*omega^2 + u*cos_theta + (M + m)*gravity*sin_theta)/(l*denom)];

%% Euler discretization
x_next = states + dt*xdot;
f_discrete = Function('f_discrete', {states, controls}, {x_next});

%% Optimization varibles
U = SX.sym('U', n_controls, Nu);
X = SX.sym('X', n_states, N);

X0_param = SX.sym('X0_param', n_states);
U_prev = SX.sym('U_prev', n_controls);

%% Problem formulation
obj = 0;
g = [];

Q = diag(state_cost);
R = input_cost;
R_delta = inc_input_cost; % weight of delta_u

g = [g; X(:,1) - X0_param];

for k = 1:N-1
    st = X(:,k);
    st_next = X(:,k+1);

    if k <= Nu
        con = U(:,k);
    else
        con = U(:,Nu);
    end

    f_value = f_discrete(st, con);
    g = [g; st_next - f_value]; % Add the dynamics

    x_ref = [0; 0; 0; 0];
    x_err = st - x_ref;
    obj = obj + x_err'*Q*x_err;

    if k <= Nu
        obj = obj + con'*R*con;

        if k == 1
            delta_u = U(:,k) - U_prev;
        else
            delta_u = U(:,k) - U(:,k-1);
        end
        obj = obj + delta_u'*R_delta*delta_u;
    end
end

% Dynamic limit: 0 <= f(x) - x <= 0 , i.e. x = f(x)
lbg = [zeros(n_states*(N),1)];
ubg = [zeros(n_states*(N),1)];

% Terminal constraint: xN' P xN <= 1
xN = X(:,N);  % last state
x_err_terminal = xN - x_ref;
obj = obj + x_err_terminal' * P * x_err_terminal;

g = [g; x_err_terminal' * P * x_err_terminal];
lbg = [lbg; 0];
ubg = [ubg; 1];


lbx = -inf*ones(n_states*(N) + n_controls*Nu,1);
ubx =  inf*ones(n_states*(N) + n_controls*Nu,1);

x_min = [-15; -pi; -10; -15];
x_max = [ 15;  pi;  10;  15];

% State constraint
for k = 0:N
    lbx(k*n_states + 1 : (k+1)*n_states) = x_min;
    ubx(k*n_states + 1 : (k+1)*n_states) = x_max;
end

% Input constraint
for k = 0:Nu-1
    idx = n_states*(N) + k*n_controls + 1;
    lbx(idx) = -20;
    ubx(idx) = 20;
end

%% Simulation
sim_time = 10;
steps = floor(sim_time/dt);

x0 = [-5; pi/6; 0; -1];
pk(1) = x0(1);
thetak(1) = x0(2);
vk(1) = x0(3);
omegak(1) = x0(4);

X_history = zeros(n_states, steps+1);
U_history = zeros(n_controls, steps);
computing_time = zeros(steps,1);
X_history(:,1) = x0;

OPT_variables = [reshape(X, n_states*(N),1); reshape(U, n_controls*Nu,1)];
nlp_prob = struct('f', obj, 'x', OPT_variables, 'g', g, 'p', [X0_param; U_prev]);

opts = struct;
opts.ipopt.print_level = 0;
opts.print_time = 0;
opts.ipopt.max_iter = 100;
opts.ipopt.tol = 1e-6;
opts.ipopt.constr_viol_tol = 1e-6;
opts.ipopt.compl_inf_tol = 1e-6;

solver = nlpsol('solver', 'ipopt', nlp_prob, opts);

t1 = cputime;
sum_computing_time = 0;
for i = 1:steps
    init_time = cputime;

    if i == 1
        u_prev = 0;
    else
        u_prev = U_history(:,i-1);
    end
    
    sol = solver('x0', zeros(length(OPT_variables),1), ...
                 'lbx', lbx, 'ubx', ubx, ...
                 'lbg', lbg, 'ubg', ubg, ...
                 'p', [X_history(:,i); u_prev]);

    sol_x = full(sol.x);
    X_sol = reshape(sol_x(1:n_states*(N)), n_states, []);    

    u_opt = sol_x(n_states*(N)+1);

    sample_time = cputime - init_time;
    sum_computing_time = sum_computing_time + sample_time;

    x_next_1 = full(f_discrete(X_history(:,i), u_opt));
    
    denom = M + m - m*cos(thetak(i))^2;

    pk(i+1) = pk(i) + dt*vk(i);
    thetak(i+1) = thetak(i) + dt*omegak(i);
    vk(i+1) = vk(i) + dt*(-m*l*sin(thetak(i))*omegak(i)^2 + m*gravity*cos(thetak(i))*sin(thetak(i)) + u_opt)/denom;
    omegak(i+1) = omegak(i) + dt*(-m*l*cos(thetak(i))*sin(thetak(i))*omegak(i)^2 + u_opt*cos(thetak(i)) + (M + m)*gravity*sin(thetak(i)))/(l*denom);
    x_next = [pk(i+1); thetak(i+1); vk(i+1); omegak(i+1)];

    X_history(:,i+1) = x_next;
    U_history(:,i) = u_opt;
    computing_time(i) = sample_time;
    J(i) = full(sol.f);
end
t2 = cputime - t1;
fprintf('Mean computing time: %.6f s\n', mean(computing_time));

%% Plots
t = 0:dt:sim_time;

figure;
subplot(5,1,1); plot(t, X_history(1,:)); ylabel('p');
subplot(5,1,2); plot(t, X_history(2,:)); ylabel('theta');
subplot(5,1,3); plot(t, X_history(3,:)); ylabel('v');
subplot(5,1,4); plot(t, X_history(4,:)); ylabel('omega');
subplot(5,1,5); stairs(t(1:end-1), U_history); ylabel('F');
xlabel('Time (s)');

figure
plot(J)
ylabel('J')

casadi.p = X_history(1,:);
casadi.theta = X_history(2,:);
casadi.v = X_history(3,:);
casadi.omega = X_history(4,:);
casadi.u = U_history;
casadi.computing_time = computing_time;
casadi.J = J;
save('casadi.mat', "casadi");
