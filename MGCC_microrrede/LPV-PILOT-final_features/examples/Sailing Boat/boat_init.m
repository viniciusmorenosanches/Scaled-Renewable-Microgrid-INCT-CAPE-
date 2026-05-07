%% Path-following of a propeller-driven sailboat (init)
%{

 This script configures and initializes the MPC controller.

%}

%%
clear all;
close all;
clc;
warning('off');

%% Controller and vehicle parameters

% Model Parameters (K and Tau) from System Identification

% Trial 1
% k = -0.005350;
% tau = 7.3659;

% Trial 2
% k = -0.006842;
% tau = 6.2893;

% Left 1
% k = -0.006247;
% tau = 1.4151;


% Right 3
% k = -0.116426;
% tau = 33.8615;

% Left 3 (slight headwind)
% k = 0.008685;
% tau = 10.9302;

% Zig-zag (slight headwind)
% k = -0.069859;
% tau = 145.6392;

% Zig-zag (slight downwind)
% k = -0.009125;
% tau = 11.6843;

% MPC test (downwind)
% k = -0.00551;
% tau = 10.2741;

% MPC test (potentiometer 1)
k = -0.014675;
tau = 9.9171;

% MPC test (potentiometer 2)
% k = -0.009493;
% tau = 6.7613;

% Controller Settings
Ts = 1; % Sample Time (s).
v = 2;  % Nominal forward velocity (m/s)

% Initial State Conditions
gama_error = 0;
theta_error = 0;
theta_dot_error = 0;
x_prev = [gama_error; theta_error; theta_dot_error];

%% Path generation
disp('Path Generation: Extrapolation');

% path generation parameters
num_samples_to_read = 50;   % Number of samples to read for initial heading
path_distance_km = 2;       % Total length of the path to be generated (km)
num_path_points = 50;       % Number of waypoints to define the path

% Hardware Communication (ESP32)
port = "COM3"; % Adapt each case for the correct port
baudrate = 115200;

disp('Connecting to ESP32 for initial telemetry...');
if ~isempty(serialportlist("available")); delete(serialportfind); end
device = serialport(port, baudrate, "Timeout", 20);
configureTerminator(device, "LF");

% Soft Reset via DTR line to "wake up" the ESP32
device.DataTerminalReady = false; % Lower DTR line
pause(0.1);                       % Short pause
device.DataTerminalReady = true;  % Raise DTR line

disp('Connection established. Collecting samples...');

% Loop to read 'num_samples_to_read' valid data packets
initial_data = [];
while size(initial_data, 1) < num_samples_to_read
    data_str = readline(device);
    if isstring(data_str) && ~isempty(data_str) && startsWith(data_str, '$')
        parts = strsplit(strtrim(erase(data_str, '$')), ',');
        if numel(parts) == 5
            % Add the 5 values as a new row to the data matrix
            initial_data(end+1, :) = str2double(parts);
            fprintf('Sample %d/%d received...\n', size(initial_data, 1), num_samples_to_read);
        end
    else
        disp('Waiting for a valid data packet...');
    end
end

% clear device; 
disp('Initial samples collected successfully.');

% Calculate Initial State (Position and Heading)
% Average the samples to get a more stable initial state
lat_start     = mean(initial_data(:, 1));
lon_start     = mean(initial_data(:, 2));
heading_start = mean(initial_data(:, 3));

fprintf('Mean initial state: Lat=%.6f, Lon=%.6f, Heading=%.4f rad\n', lat_start, lon_start, heading_start);

% Calculate Destination Point by Extrapolation
[lat_target, lon_target] = calculate_destination_point(lat_start, lon_start, heading_start, path_distance_km);
fprintf('Calculated destination point: Lat=%.6f, Lon=%.6f\n', lat_target, lon_target);

% Generate Path and Convert to Local Coordinates
% (straight line in GPS coordinates)
path_lat_gps = linspace(lat_start, lat_target, num_path_points);
path_lon_gps = linspace(lon_start, lon_target, num_path_points);

% The boat's starting point becomes the origin (0,0) of the local map
lat0 = lat_start;
lon0 = lon_start;

% Convert the entire GPS path to local XY coordinates (meters)
[path_x, path_y] = latlon2local(path_lat_gps, path_lon_gps, lat0, lon0);

% Calculate Path Heading Angle
path_heading = heading_start * ones(size(path_x));

%% MPC controller configuration
disp('Continuing with MPC initialization...');

%% Create MPC object
N = 50;
N_h_ctr = N;
state_cost = [0.05, 1, 0.0]; % [gama, theta, theta_dot]
input_cost = 1; 
inc_input_cost = 0; % Incremental input cost
mpc = init_mpc(N,N_h_ctr);

%%
v = 2; 
A = [0 v*cos(0) 0; 0 0 1;0 0 1/tau];
B = [0; 0; k/tau];
Bd = [0; 0; 0];
C = eye(3);
mpc = init_mpc_system(mpc,eye(3)+Ts*A,Ts*B,Ts*Bd,C,[0],[]);

%% Pre-computing Aeq (for LPV)
% LPV matrices
A0 = eye(3) + Ts*[0 0 0;
                  0 0 1;
                  0 0 1/tau];
A1 = Ts*[0 1 0;
         0 0 0;
         0 0 0];
B0 = Ts*[0; 0; k/tau];
B1 = Ts*[0; 0; 0];
Bd0 = Ts*[0; 0; 0];
Bd1 = Ts*[0; 0; 0];
n_rho = 1;
[mpc.Aeq0, mpc.Aeq1] = init_mpc_Aeq(mpc, A0, B0, n_rho, A1, B1);
[mpc.beq0, mpc.beq1, mpc.beq_ff0, mpc.beq_ff1] = init_mpc_beq(mpc, A0, Bd0, n_rho, A1, Bd1);

%% Constraints
x_min = [-100; -2*pi; -2*pi];
x_max = [ 100; 2*pi; 2*pi];
tightening = 0.1; % 0 <= tightening <= 1
mpc = init_mpc_state_cnstr(mpc,x_min,x_max,tightening);

u_min = -5;
u_max = 5;
mpc = init_mpc_u_cnstr(mpc,u_min,u_max);

du_min = -1;
du_max = 1;
mpc = init_mpc_delta_u_cnstr(mpc,du_min,du_max);

% y_min = [];
% y_max = [];
%mpc = init_mpc_output_cnstr(mpc,y_min,y_max);

%% General Linear Inequalities (Unused)
Ci = [];
Di = [];
Ddi = [];
yi_min = [];
yi_max = [];
%mpc = init_mpc_general_lin_ineq_cnstr(mpc,yi_min,yi_max,Ci,Di,Ddi);

%% Terminal Ingredients

%x_ter_min = [];
%x_ter_max = [];
%mpc = init_mpc_ter_state_cnstr(mpc,x_ter_min,x_ter_max);

% Terminal cost
Qx = diag(state_cost);
Ru = input_cost;
ter_constraint = 1;
x_ref_is_y = 1;

% System matrices linearized around the equilibrium point: theta = 0
A_d = eye(3) + Ts*[0 v*cos(0) 0; 0 0 1; 0 0 1/tau];
B_d = Ts*[0; 0; k/tau];
Q_dlqr = diag(state_cost);
R_dlqr = input_cost;
%[K, P, ~] = dlqr(A_d, B_d, Q_dlqr, R_dlqr);
[mpc] = init_mpc_ter_ingredients_dlqr(mpc,Qx,Ru,ter_constraint,x_ref_is_y);

%% Costs
Qe = diag(state_cost);
mpc = init_mpc_Tracking_cost(mpc,Qe);

Ru = input_cost;
mpc = init_mpc_Control_cost(mpc,Ru);

Rdu = inc_input_cost;
mpc = init_mpc_DiffControl_cost(mpc,Rdu);

%% Cost Matrices for Analysis
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

%% Solver Settings
% Set QP solver to be more aggressive
mpc.t = 5;

%% Simulation Initial Conditions (Warm Start)
u_prev = 0;
d = [0];
x_ref = [0; 0; 0];
x0 = init_mpc_warm_start(mpc,x_prev,u_prev,d,x_ref);

%%
disp('MPC initialization finished.');