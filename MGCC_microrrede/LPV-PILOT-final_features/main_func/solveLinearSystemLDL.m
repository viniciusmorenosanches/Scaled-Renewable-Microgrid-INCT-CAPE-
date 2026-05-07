function x = solveLinearSystemLDL(A, b)
    % Solves Ax = b using LDL' factorization.

    [L, D] = ldlFactorization(A);
    y = forwardSubstitution(L, b);
    z = diagonalSolve(D, y);
    x = backSubstitution(L', z);
end

function [L, D] = ldlFactorization(A)
    n = size(A, 1);
    L = eye(n);
    D = zeros(n, 1);
    
    for i = 1:n
        D(i) = A(i, i) - L(i, 1:i-1) * (D(1:i-1) .* L(i, 1:i-1)');
        
        for j = i+1:n
            L(j, i) = (A(j, i) - L(j, 1:i-1) * (D(1:i-1) .* L(i, 1:i-1)')) / D(i);
        end
    end
    D = diag(D);
end

function y = forwardSubstitution(L, b)
    n = length(b);
    y = zeros(n, 1);
    
    for i = 1:n
        y(i) = b(i) - L(i, 1:i-1) * y(1:i-1);
    end
end

function z = diagonalSolve(D, y)
    z = y ./ diag(D);
end

function x = backSubstitution(U, y)
    n = length(y);
    x = zeros(n, 1);
    
    for i = n:-1:1
        x(i) = y(i) - U(i, i+1:n) * x(i+1:n); 
    end
end