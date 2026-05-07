%% Path-following of a propeller-driven sailboat (main)
%{
 This script implements a Nonlinear Model Predictive Control (MPC) 
 algorithm for a propeller-driven sailboat path following, operating 
 in a Hardware-in-the-Loop (HIL) scheme.
%}

%% Configuration and initialization
clear; clc; close all;

% Load vehicle and path parameters
boat_init 

% Create UI for manual stop
fig = uifigure('Name', 'MPC Control', 'Position', [100 100 350 100]);
d = uiprogressdlg(fig, 'Title', 'Running HIL Control', 'Message', 'Close this window to STOP execution.');

%% Control and filter initialization

% MPC State initialization
u = []; % Control input history
gama_error(1) = x_prev(1); % Lateral error
theta_error(1) = x_prev(2); % Heading error
theta_dot_error(1) = x_prev(3); % Heading rate error
x(1) = lon0; y(1) = lat0; theta(1) = path_heading(1);
x(1) = 0; y(1) = 0; % Start at local origin
ref_theta(1) = path_heading(1);
t(1) = 0; % Time vector
meas_Iu = []; % Measured integral 'u' (or measured rudder angle)
w(1) = 0;
extended_ref_Iu = [];
r_gama = 0; r_theta = 0; r_theta_dot = 0; % Error references (usually zero)

% Filter parameters
windowSize = 5; % Size of the moving average window (N)
% Buffer for N samples and 5 variables (lat, lon, head, int_u, u)
dataBuffer = zeros(windowSize, 5); 
isInitialized = false; % Flag to indicate if the buffer is full

disp('Starting path-following');

%% Main control loop (Synchronous HIL)
% The loop continues as long as the UI figure 'fig' exists.
i = 0;
while ishandle(fig)
    init_ts = tic; % Start timer for sample time enforcement
    i = i + 1;
    fprintf('\n--- Control Step %d/%d ---\n', i, steps);
    
    %%%%%%%%%%%%%%%%%% MPC core logic %%%%%%%%%%%%%%%%%%
    if abs(theta_error(i) - 0) < 1e-5; rho_1 = 0; else; rho_1 = v*sin(theta_error(i))/theta_error(i); end
    Pk = repmat([rho_1], N);
    d = [0]; xref = [r_gama; r_theta; r_theta_dot];
    mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0, Pk, n_rho, mpc.Aeq1, Bd1);
    mpc = update_mpc_precomputed_beq(mpc, x_prev, Pk, n_rho, mpc.beq0, mpc.beq1, mpc.beq_ff0, mpc.beq_ff1);
    [u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    % The control output is the reference for the rudder angle (inverted)
    ref_Iu = -u_prev;
    
    %%%%%%%%%% Batch acquisition and filtering block %%%%%%%%%%
    
    % Send Command to Hardware (ESP32)
    comando_tx = sprintf('C%.4f', ref_Iu);
    writeline(device, comando_tx);
    fprintf('Command sent: %s\n', comando_tx);
    
    % Parameters for the batch acquisition strategy
    N_samples_per_step = 8; % Number of samples to collect per control step
    outlier_threshold_meters = 5.0; % Position outlier threshold (Haversine dist)
    alpha = 0.4; % Exponential Moving Average (EMA) factor for heading
    
    % Collect a 'Batch' of 'N' Samples
    fprintf('Collecting a batch of %d samples from ESP32...\n', N_samples_per_step);
    flush(device); % Clear serial buffer
    dataBatch = zeros(N_samples_per_step, 5);
    samples_collected = 0;
    block_acquisition_success = false; % Flag to track if the full batch was received
    
    start_wait_time_total = tic; 
    max_wait_time_total = 20; % Max time to wait for the entire batch
    
    while samples_collected < N_samples_per_step && toc(start_wait_time_total) < max_wait_time_total
        data_str = ""; 
        start_wait_time_ind = tic; 
        max_wait_time_ind = 5; % Max time to wait for a *single* sample
        
        % Wait for a valid data string (starting with '$')
        while ~(startsWith(data_str, '$')) && toc(start_wait_time_ind) < max_wait_time_ind
            if device.NumBytesAvailable > 0; data_str = readline(device); end
            pause(0.01); % Small pause to prevent busy-waiting
        end
        
        % If a valid string is received, parse and store it
        if startsWith(data_str, '$')
            samples_collected = samples_collected + 1;
            parts = strsplit(strtrim(erase(data_str, '$')), ',');
            dataBatch(samples_collected, :) = str2double(parts);
            fprintf(' -> Sample %d/%d received.\n', samples_collected, N_samples_per_step);
        else
            % Individual sample timeout
            fprintf(2, 'WARNING: Timeout while waiting for sample %d.\n', samples_collected + 1);
            break; % Exit the inner loop and fail the batch
        end
    end
    
    % Check if the full batch was collected
    if samples_collected == N_samples_per_step
        block_acquisition_success = true;
    end
    
    % Process the Collected Batch
    if block_acquisition_success
        % Heading: Apply Exponential Moving Average (EMA) for smoothing
        
        % Calculate the mean heading from the current batch
        mean_heading_batch = mean(dataBatch(:, 3)); 
        % Get the previously filtered and stored heading value (from step i)
        previous_filtered_heading = theta(i); 
    
        % Apply the EMA formula, handling angle wrap-around
        delta_heading = wrapToPi(mean_heading_batch - previous_filtered_heading);
        filtered_heading = previous_filtered_heading + alpha * delta_heading;
        
        % Ensure the final result remains in the [0, 2*pi] convention
        filtered_heading = wrapTo2Pi(filtered_heading);
        
        % Position: Use the last value, but check for outliers
        last_position_lat = dataBatch(end, 1);
        last_position_lon = dataBatch(end, 2);
        
        % Check against the batch average for outlier detection
        avg_position_lat = mean(dataBatch(:, 1));
        avg_position_lon = mean(dataBatch(:, 2));
        distance_m = haversine_distance(last_position_lat, last_position_lon, avg_position_lat, avg_position_lon);
        
        if distance_m > outlier_threshold_meters
            % Outlier detected: use the batch average instead
            final_lat = avg_position_lat;
            final_lon = avg_position_lon;
            fprintf(2, 'WARNING: Position outlier detected (dist=%.1fm). Using batch mean position.\n', distance_m);
        else
            % Data looks good: use the last reported position
            final_lat = last_position_lat;
            final_lon = last_position_lon;
        end
        
        % Other variables (using the full batch for storage)
        %integral_u_filtered = mean(dataBatch(:, 4));
        %motor_voltage_filtered = mean(dataBatch(:, 5));
        integral_u_filtered = dataBatch(:, 4)';
        motor_voltage_filtered = dataBatch(:, 5)';
    
        % Update state with filtered data
        lat_real(i) = final_lat;
        lon_real(i) = final_lon;
        % Convert GPS (lat/lon) to local (x/y) coordinates
        [x(i+1), y(i+1)] = latlon2local(final_lat, final_lon, lat0, lon0); 
        theta(i+1) = filtered_heading;
        meas_Iu = [meas_Iu, integral_u_filtered];
        u = [u, motor_voltage_filtered];
        Iu(i) = ref_Iu;
        extended_ref_Iu = [extended_ref_Iu, ref_Iu*ones(size(integral_u_filtered))];
        fprintf('Filtered Data: x=%.2fm, y=%.2fm | Head=%.2frad\n', x(i+1), y(i+1), theta(i+1));
    else
        % This is a critical failure; the HIL loop is broken
        fprintf(2, 'CRITICAL ERROR: Failed to collect sample batch. Aborting loop.\n');
        break; % Exit the main 'while' loop
    end
    
    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    
    
    %% State update
    
    % Update error states
    [projected_x, projected_y, projected_heading] = orthogonal_projection(x(i+1),y(i+1),path_x,path_y,path_heading);
    projected_heading = wrapTo2Pi(projected_heading);
    [gama_error(i+1), theta_error(i+1)] = compute_error(x(i+1),y(i+1),theta(i+1), projected_x, projected_y, projected_heading);
    gama_error(i+1) = gama_error(i+1);
    
    % Calculate error rate using finite difference
    theta_dot_error(i+1) = (theta_error(i+1) - theta_error(i))/Ts;
    
    % Update state for the next MPC calculation
    x_prev = [gama_error(i+1); theta_error(i+1); theta_dot_error(i+1)];
    ref_theta(i+1) = projected_heading;
    
    % Synchronous loop timing (sampling time)
    if toc(init_ts) < Ts
        time_to_wait = Ts - toc(init_ts);
        pause(time_to_wait);
    end
    % Log the actual time elapsed for this step
    t(i+1) = t(i) +  toc(init_ts);
end

%% Results
disp('Control loop finished. Closing communication.');
if exist('device', 'var')
    clear device;
end
close all % Close all figures (including the UI)

% Data cleanup
x = x(2:end);
y = y(2:end);

% GPS Trajectory (geoplot)
figure('Name', 'Collected GPS Trajectory');
geoplot(lat_real, lon_real, '.-'); % Plot actual trajectory on a map
hold on;
geoplot(path_lat_gps, path_lon_gps, '--'); % Plot reference path
legend('Actual Trajectory', 'Reference Path');
title('Collected GPS Trajectory');
geobasemap satellite;

% Auto-zoom logic for geoplot
% Calculate limits based on the actual trajectory
min_lat = min(lat_real);
max_lat = max(lat_real);
min_lon = min(lon_real);
max_lon = max(lon_real);
% Add a 20% margin
range_lat = max_lat - min_lat;
range_lon = max_lon - min_lon;
padding_lat = range_lat * 0.20;
padding_lon = range_lon * 0.20;
% Ensure a minimum margin (e.g., 20m, converted to degrees)
if padding_lat == 0, padding_lat = 0.0002; end 
if padding_lon == 0, padding_lon = 0.0002; end 
% Apply new limits
geolimits([min_lat - padding_lat, max_lat + padding_lat],[min_lon - padding_lon, max_lon + padding_lon])

% Trajectory plot (local coordinates in meters)
figure('Name', 'Trajectory in Local Plane (Meters) with Zoom');
plot(path_x, path_y, '--', 'Color', [1 0.0 0.0], 'DisplayName', 'Planned Path (m)');
hold on;
plot(x, y, 'b.-', 'LineWidth', 1.5, 'DisplayName', 'Actual Trajectory (m)');
hold off;
axis equal; grid on; legend;
title('Trajectory in Local Coordinates');
xlabel('X (meters)'); ylabel('Y (meters)');

% Auto-zoom logic for local plot
min_x_zoom = min(x); max_x_zoom = max(x);
min_y_zoom = min(y); max_y_zoom = max(y);
range_x_zoom = max_x_zoom - min_x_zoom; range_y_zoom = max_y_zoom - min_y_zoom;
pad_x = range_x_zoom * 0.2 + 5; % Add 20% margin + 5 meters
pad_y = range_y_zoom * 0.2 + 5;
xlim([min_x_zoom - pad_x, max_x_zoom + pad_x]);
ylim([min_y_zoom - pad_y, max_y_zoom + pad_y]);

% Heading plot
figure('Name', 'Heading Angle');
plot(t, ref_theta)
hold on
plot(t, theta)
legend('Reference Heading', 'Actual Heading')
ylabel('Heading Angle (rad)')
xlabel('Time (s)')
grid on;

% Control Input and Output Plot
t_interpolated = linspace(t(1), t(end), length(meas_Iu));
figure('Name', 'Control Signals');
subplot(2,1,1)
plot(t(2:end), Iu);
hold on
plot(t_interpolated, meas_Iu);
legend('Reference Rudder Angle', 'Measured Rudder Angle') 
title('Rudder Control');
ylabel('Rudder Angle (º)');
grid on;

subplot(2,1,2)
plot(t_interpolated, u);
title('Motor Voltage');
xlabel('Time (s)');
ylabel('Motor Voltage (V)');
grid on;

% Error plots
figure('Name', 'Control Errors');
subplot(3,1,1)
plot(t, gama_error)
ylabel('gamma_error (lateral)')
grid on;
subplot(3,1,2)
plot(t, theta_error)
ylabel('theta_error (heading)')
grid on;
subplot(3,1,3)
plot(t, theta_dot_error)
ylabel('theta_dot_error (heading rate)')
xlabel('Time (s)')
grid on;

%% Save results
out.x = x;
out.y = y;
out.lat_real = lat_real;
out.lon_real = lon_real;
out.theta = theta;
out.ref_Iu = Iu;
out.meas_Iu = meas_Iu;
out.gama_error = gama_error;
out.theta_error = theta_error;
out.theta_dot_error = theta_dot_error;
out.u = u;
out.path_x_meters = path_x;
out.path_y_meters = path_y;
out.path_lat_gps = path_lat_gps;
out.path_lon_gps = path_lon_gps;
out.path_heading = path_heading;
out.Ts = Ts;
out.t = t;
save('test_1_results', 'out');