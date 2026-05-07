function [beq, beq_ff] = pre_compute_beq(mpc, A, Bd)

    if isempty(Bd)
        beq(1:mpc.nx, 1:mpc.nx) = -A;
    else
    
    nd = size(Bd,2);

        for k = 0:mpc.N-1
            switch k 
                case 0
                    beq(1:mpc.nx, 1:mpc.nx) = -A;
                    beq_ff(1:mpc.nx, 1:nd) = - Bd;            
                otherwise
                    beq(k*mpc.nx+1:(k+1)*mpc.nx, 1:mpc.nx) = zeros(mpc.nx, mpc.nx);
                    beq_ff(k*mpc.nx+1:(k+1)*mpc.nx, 1:nd) = - Bd;
            end
        end
    end
end