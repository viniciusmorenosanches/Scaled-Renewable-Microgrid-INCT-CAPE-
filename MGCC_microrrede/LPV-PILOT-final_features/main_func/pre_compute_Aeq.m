function Aeq = pre_compute_Aeq(A,B,N,N_h_ctr,Nx,Nu,nx,nu)
% Similar to genEqualities.m, but with -zeros(nx) instead of -eye(nx)

Aeq = zeros(Nx,Nx+Nu);
for k = 0:N-1

    if k == 0
        Aeq(1:nx,1:nu+nx) = [B -zeros(nx)];
    
    elseif k < N_h_ctr
        Aeq(k*nx+1:(k+1)*nx,(nu+nx)*k+1-nx:(nu+nx)*(k+1)) = [A B -zeros(nx)];

    else
        Aeq(k*nx+1:(k+1)*nx,(nx+nu)*(N_h_ctr-1)+1:(nx+nu)*(N_h_ctr-1)+nu) = ...
            B;

        Aeq(k*nx+1:(k+1)*nx,(nx+nu)*(N_h_ctr-1)+nu+nx*(k-(N_h_ctr))+1:...
            (nx+nu)*(N_h_ctr-1)+nu+nx*(k-(N_h_ctr-2))) = ...
            [A -zeros(nx)];

    end
end

end