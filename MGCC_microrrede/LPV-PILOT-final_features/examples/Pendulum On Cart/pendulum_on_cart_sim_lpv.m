% Load init script defining the mpc problem
pendulum_on_cart_init

%clear p_dat theta_dat v_dat omega_dat uk rk
%clc

pk(1) = x_prev(1);
thetak(1) = x_prev(2);
vk(1) = x_prev(3);
omegak(1) = x_prev(4);

r_p = 0;
r_theta = 0;
r_v = 0;
r_omega = 0;

sim_time = 10; %s
steps = ceil(sim_time/Ts);

t1 = cputime;
sum_computing_time = 0;
for i = 1:steps
    % Update LPV model
    rho_1 = ( -m*l*sin(thetak(i))*omegak(i) )/(M + m - m*cos(thetak(i))^2);
    rho_3 = -m*cos(thetak(i))*sin(thetak(i))*omegak(i) / (M + m - m*cos(thetak(i))^2);
    rho_5 = 1/(M + m - m*cos(thetak(i))^2);
    rho_6 = cos(thetak(i))/( l * (M + m - m*cos(thetak(i))^2) );
    
    if abs(thetak(i) - 0) < 1e-6
        rho_2 = 0;
        rho_4 = 0;
    else
        rho_2 = m*g*cos(thetak(i))*sin(thetak(i)) / ( (M + m - m*cos(thetak(i))^2) * thetak(i) );
        rho_4 = (M + m)*g*sin(thetak(i)) / ( (M + m - m*cos(thetak(i))^2) * (l*thetak(i)) );
    end
    
    Pk = repmat([rho_1; rho_2; rho_3; rho_4; rho_5; rho_6], N); % Frozen gain schedulling
    
    d = [0];
    
    xref = [r_p;r_theta; r_v;r_omega];
    
    init_computing_time = cputime;
    mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0, Pk, n_rho, ...
                            mpc.Aeq1, mpc.Aeq2, mpc.Aeq3, mpc.Aeq4, mpc.Aeq5, mpc.Aeq6, ...
                            Bd1, Bd2, Bd3, Bd4, Bd5, Bd6);
    % The function 'update_mpc_beq' can be replaced by
    % update_mpc_precomputed_beq
    mpc = update_mpc_precomputed_beq(mpc, x_prev, d, Pk, n_rho, ...
        mpc.beq0, mpc.beq1, mpc.beq2, mpc.beq3, mpc.beq4, mpc.beq5, mpc.beq6, ...
        mpc.beq_ff0, mpc.beq_ff1, mpc.beq_ff2, mpc.beq_ff3, mpc.beq_ff4, mpc.beq_ff5, mpc.beq_ff6);
    
    [u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);
    sample_time = cputime - init_computing_time;
    sum_computing_time = sum_computing_time + sample_time;
    
    Nu = mpc.N_ctr_hor;
    x_prediction = [];
    u_prediction = [];
    k = 1;
    for v = 1:Nu
        u_prediction = [u_prediction;
                        x0(k)];
        x_prediction = [x_prediction;
                        x0(k + 1);
                        x0(k + 2);
                        x0(k + 3);
                        x0(k + 4)];
        k = k + 5;
    end
    x_prediction = [x_prediction;
                    x0(k:end)];
    
    J = x_prediction'*Qe_cost*x_prediction + u_prediction'*Ru_cost*u_prediction + sum(diff(u_prediction).^2); 
    
    denom = M + m - m*cos(thetak(i))^2;

    pk(i+1) = pk(i) + Ts*vk(i);
    thetak(i+1) = thetak(i) + Ts*omegak(i);
    vk(i+1) = vk(i) + Ts*(-m*l*sin(thetak(i))*omegak(i)^2 + m*g*cos(thetak(i))*sin(thetak(i)) + u_prev)/denom;
    omegak(i+1) = omegak(i) + Ts*(-m*l*cos(thetak(i))*sin(thetak(i))*omegak(i)^2 + u_prev*cos(thetak(i)) + (M + m)*g*sin(thetak(i)))/(l*denom);
    
    x_prev = [pk(i+1);thetak(i+1);vk(i+1);omegak(i+1)];
    
    p_dat(:,i) = pk(i+1);
    theta_dat(:,i) = thetak(i+1);
    v_dat(:,i) = vk(i+1);
    omega_dat(:,i) = omegak(i+1);
    uk(i) = u_prev;
    r_p_k(i) = r_p;
    r_theta_k(i) = r_theta;
    r_v_k(i) = r_v;
    r_omega_k(i) = r_omega;
    computing_time(i) = sample_time;
    J_dat(i) = J;

end
t2 = cputime-t1;
t2/steps
steps/t2

pilot.p = pk;
pilot.theta = thetak;
pilot.v = vk;
pilot.omega = omegak;
pilot.u = uk;
pilot.computing_time = computing_time;
pilot.J = J_dat;

save('pilot.mat', "pilot");

close all

figure
subplot(4,1,1)
plot(r_p_k)
hold on
plot(pk)
legend('ref', 'out')
ylabel('pk')

subplot(4,1,2)
plot(r_theta_k)
hold on
plot(thetak)
legend('ref', 'out')
ylabel('thetak')

subplot(4,1,3)
plot(r_v_k)
hold on
plot(vk)
legend('ref', 'out')
ylabel('vk')

subplot(4,1,4)
plot(r_omega_k)
hold on
plot(omegak)
legend('ref', 'out')
ylabel('omegak')

figure
plot(uk)
grid on
ylabel('u')

figure
plot(J_dat)
ylabel('J')


% figure
% plot(Ts*(1:i),rk,'r',Ts*(1:i),p_dat,'g',Ts*(1:i),v_dat,'b', Ts*(1:i),omega_dat, Ts*(1:i),theta_dat)
% grid on
% ylim([0 1])
% legend('ref','pk','vk', 'omegak', 'thetak')

% figure
% plot(Ts*(1:i),uk,Ts*(1:i-1),diff(uk))
% grid on
% legend('U','Delta U')