function varargout = init_mpc_beq(mpc,A0,Bd0, n_rho, varargin)

    % if (length(varargin)/n_rho ~= 2)
    %     error("ERROR: You must pass 'n_rho' matrices for A and Bd (Bd is optional)");
    % end

    varargout = cell(1, 2*n_rho + 2);
    if ~isempty(A0) && ~isempty(Bd0)
        % [beq0, beq_ff0]
        [varargout{1}, varargout{n_rho + 2}] = pre_compute_beq(mpc, A0, Bd0);

        for i = 1:n_rho
            % [beqn, beq_ffn]
            [varargout{i + 1}, varargout{i + n_rho + 2}] = pre_compute_beq(mpc, varargin{i},varargin{i + n_rho}); 
        end
    end
end