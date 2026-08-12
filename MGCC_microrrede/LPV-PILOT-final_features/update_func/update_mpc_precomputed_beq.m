function mpc = update_mpc_precomputed_beq(mpc,x_prev,d, Pk, n_rho, varargin)
    % beq = bd0
    beq = varargin{1};
    beq_ff = varargin{1 + n_rho + 1};
    for i = 1:n_rho
        beq = beq + varargin{i + 1}*Pk(i);
        beq_ff = beq_ff + varargin{i + n_rho + 2}*Pk(i);
    end
    mpc.beq = beq*x_prev + beq_ff*d;
end