function mpc = update_mpc_sys_dynamics(mpc,A0,B0,Bd0, Pk, n_rho, varargin)
% This function might be unused from now

    if (length(Pk) ~= mpc.N*n_rho)
        warning("WARNING: The dimension of 'Pk' must match with the prediction horizon ('mpc.N') times the number of schedulling variables 'n_rho'.")
    end
    if (length(varargin)/n_rho ~= 3 && length(varargin)/n_rho ~= 2)
        error("ERROR: You must pass 'n_rho' matrices for A, B and Bd (Bd is optional)");
    end
    
    if ~isempty(A0) && ~isempty(B0)
        A = A0;
        B = B0;
        
        for i = 1:n_rho
            A = A + varargin{i}*Pk(i);
            B = B + varargin{i + n_rho}*Pk(i);
        end
    
        mpc.A = A;
        mpc.B = B;
    
        
        mpc.Aeq = genEqualities(A,B,mpc.N,mpc.N_ctr_hor,mpc.Nx,mpc.Nu,mpc.nx,mpc.nu);
    end
    
    if ~isempty(Bd0)  
        Bd = Bd0;
        
        for i = 1:n_rho
            Bd = Bd + varargin{i + 2*n_rho}*Pk(i);
        end
    
        mpc.Bd = Bd;
    end

end