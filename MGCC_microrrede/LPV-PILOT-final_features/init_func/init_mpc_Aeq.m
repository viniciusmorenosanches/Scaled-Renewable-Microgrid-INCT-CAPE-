function varargout = init_mpc_Aeq(mpc,A0,B0, n_rho, varargin)

    % if (length(varargin)/n_rho ~= 3 && length(varargin)/n_rho ~= 2)
    %     error("ERROR: You must pass 'n_rho' matrices for A, B and Bd (Bd is optional)");
    % end

    varargout = cell(1, n_rho+1);
    if ~isempty(A0) && ~isempty(B0)
        varargout{1} = genEqualities(A0,B0,mpc.N,mpc.N_ctr_hor,mpc.Nx,mpc.Nu,mpc.nx,mpc.nu);

        for i = 1:n_rho
            varargout{i + 1} = pre_compute_Aeq(varargin{i},varargin{i + n_rho},mpc.N,mpc.N_ctr_hor,mpc.Nx,mpc.Nu,mpc.nx,mpc.nu);
        end
    end
end