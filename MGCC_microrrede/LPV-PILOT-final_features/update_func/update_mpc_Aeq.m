function mpc = update_mpc_Aeq(mpc,Aeq0, Bd0, Pk, n_rho, varargin)

if (length(Pk) ~= mpc.N*n_rho)
    warning("WARNING: The dimmension of 'Pk' must match with the prediction horizon ('mpc.N') times the number of schedulling variables 'n_rho'.")
end

if ~isempty(Aeq0)
    Aeq = Aeq0;
    
    for i = 1:n_rho
        Aeq = Aeq + varargin{i}*Pk(i);
    end

    mpc.Aeq = Aeq;
end

if ~isempty(Bd0)  
    Bd = Bd0;
    
    for i = 1:n_rho
        Bd = Bd + varargin{i + n_rho}*Pk(i);
    end

    mpc.Bd = Bd;
end

end