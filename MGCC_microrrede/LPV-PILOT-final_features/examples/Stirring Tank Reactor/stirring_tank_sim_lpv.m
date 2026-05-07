% Load init script defining the mpc problem
stirring_tank_init

clear c_dat v_dat uk rk
ck = x_prev(1);
vk = x_prev(2);

r_c = 0.65;
r_v = 0.53;

f_c = ck;
f_v = vk;
tau = 15;

t1 = cputime;
for i = 1:2400

f_c = f_c + Ts*(-f_c/tau+r_c/tau);
f_v = f_v + Ts*(-f_v/tau+r_v/tau);

% Set LPV dynamics
rho_1 = exp(-M/vk);
rho_2 = ck*exp(-M/vk)/(vk^2);
rho_3 = vk;
Pk = repmat([rho_1; rho_2; rho_3], N); % Frozen gain schedulling

d = [1;vk];

mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0, Pk, n_rho, mpc.Aeq1, mpc.Aeq2, mpc.Aeq3, Bd1, Bd2, Bd3);
% The function 'update_mpc_beq' can be replaced by
% update_mpc_precomputed_beq
mpc = update_mpc_precomputed_beq(mpc, x_prev, d, Pk, n_rho, ...
    mpc.beq0, mpc.beq1, mpc.beq2, mpc.beq3, mpc.beq_ff0, mpc.beq_ff1, mpc.beq_ff2, mpc.beq_ff3);


xref = [f_c;f_v];

[u_prev,J,x0] = mpc_solve(x0,x_prev,u_prev,xref,d,mpc,[],[],[]);

ck = ck + Ts*((1-ck)/theta_f - k*ck*exp(-M/vk));
vk = vk + Ts*((xf-vk)/theta_f + k*ck*exp(-M/vk)-alpha*u_prev*(vk-xc));

x_prev = [ck;vk];

c_dat(:,i) = ck;
v_dat(:,i) = vk;
Jk(i) = J;
uk(i) = u_prev;
rk(i) = f_c;

end
t2 = cputime-t1;
t2/2400
2400/t2

close all

figure
plot(Ts*(1:i),rk,'r',Ts*(1:i),c_dat,'g',Ts*(1:i),v_dat,'b')
grid on
ylim([0 1])
legend('c ref','ck','vk')

figure
plot(Ts*(1:i),uk,Ts*(1:i-1),diff(uk))
grid on
legend('U','Delta U')