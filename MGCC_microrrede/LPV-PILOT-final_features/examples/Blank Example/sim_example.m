% Load init script defining the mpc problem
stirring_tank_init

% Clear state x1, x2, xn..., control input and reference
clear x1_dat x2_dat xn_dat uk rk_x1 rk_x2

x1 = x_prev(1);
x2 = x_prev(2);
xn = x_prev(n);
...

r_x1 = ...;
r_x2 = ...;
r_xn = ...;

% Reference filter
f_x1 = x1;
f_x2 = x2;
tau = ...;  % Time constant

% Number of schedulling variables
n_rho = ...;

% Number of simulation iterations
iters = ...;

t1 = cputime; % Start measuring time
for i = 1:iters

    % Update the filter reference
    f_x1 = f_x1 + Ts*(-f_x1/tau+r_x1/tau);
    f_x2 = f_x2 + Ts*(-f_x2/tau+r_x2/tau);
    
    % Set LPV dynamics
    rho_1 = ...;
    rho_2 = ...;
    rho_n = ...;
    Pk = repmat([rho_1; rho_2; rho_n], N); % Frozen gain schedulling
    
    % Update LPV model
    % A = A0 + A1*rho_1 + A2*rho_2 + An*rho_n ...
    A0 = eye(2) + Ts*[... ...; 
                      ... ...];
    A1 = Ts*[... ...; 
             ... ...];
    A2 = Ts*[... ...;
             ... ...];
    An = Ts*[... ...;
             ... ...];
    
    % B = B0 + B1*rho_1 + B2*rho_2 + Bn*rho_n ...
    B0 = Ts*[...; ...];
    B1 = Ts*[...; ...];
    B2 = Ts*[...; ...];
    
    % Bd = Bd0 + Bd1*rho_1 + Bd2*rho_2 + Bdn*rho_n ...
    Bd0 = Ts*[... ...;
              ... ...];
    Bd1 = Ts*[... ...; ... ...];
    
    Bd2 = Ts*[... ...; ... ...];
    
    % TO DO: Pass just Pk and let A0, B0, Bd0, An, Bn and Bdn fixed
    mpc = update_mpc_sys_dynamics(mpc, A0, B0, Bd0, Pk, n_rho, A1, A2, An, ... B1, B2, Bn, ..., Bd1, Bd2, Bdn...);
    
    d = [...;...];
    
    xref = [f_x1;f_x2];
    
    [u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);
    
    x1 = x1 + Ts*(...);
    x2 = x2 + Ts*(...);
    xn = xn + Ts*(...);
    
    x_prev = [x1;x2;...];
    
    x1_dat(:,i) = x1;
    x2_dat(:,i) = x2;
    Jk(i) = J;
    uk(i) = u_prev;
    rk_x1(i) = f_x1;
    rk_x2(i) = f_x2;

end
t2 = cputime-t1;
t2 % Print the simulation time
t2/iters % Print the mean time for each sample

close all

% Plot the graphics
figure
plot(Ts*(1:i),rk_x1,'r_x1',Ts*(1:i),rk_x2,'r_x2',Ts*(1:i),x1_dat,'g',Ts*(1:i),x2_dat,'b')
grid on
ylim([0 1])
legend('x1 ref', 'x2 ref','x1','x2')

figure
plot(Ts*(1:i),uk,Ts*(1:i-1),diff(uk))
grid on
legend('U','Delta U')