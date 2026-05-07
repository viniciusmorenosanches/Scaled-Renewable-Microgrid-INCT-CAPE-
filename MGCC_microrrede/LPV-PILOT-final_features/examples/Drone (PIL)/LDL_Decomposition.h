#ifndef LDL_DECOMPOSITION_H
#define LDL_DECOMPOSITION_H

// LDL^T Decomposition
void ldlDecomposition(float** A, float** L, float* D, int n) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            L[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        for (int k = 0; k < i; ++k)
            sum += L[i][k] * L[i][k] * D[k];
        
        D[i] = A[i][i] - sum;
        
        for (int j = i + 1; j < n; ++j) {
            float sum2 = 0.0f;
            for (int k = 0; k < i; ++k)
                sum2 += L[j][k] * L[i][k] * D[k];

            L[j][i] = (A[j][i] - sum2) / D[i];
        }
    }
}

void forwardSubstitution_LDL(float** L, float* b, float* y, int n) {
    for (int i = 0; i < n; ++i) {
        y[i] = b[i];
        for (int j = 0; j < i; ++j)
            y[i] -= L[i][j] * y[j];
    }
}

void diagonalSolve(float* D, float* y, float* z, int n) {
    for (int i = 0; i < n; ++i)
        z[i] = y[i] / D[i];
}

void backSubstitution_LDL(float** L, float* z, float* x, int n) {
    for (int i = n - 1; i >= 0; --i) {
        x[i] = z[i];
        for (int j = i + 1; j < n; ++j)
            x[i] -= L[j][i] * x[j];  // Lᵗ
    }
}

void solveSystemLDL(float** A, float* b, float* x, int n) {
    // Allocate heap
    float** L = (float**) malloc(n * sizeof(float*));
    for (int i = 0; i < n; ++i)
        L[i] = (float*) calloc(n, sizeof(float));

    float* D = (float*) malloc(n * sizeof(float));
    float* y = (float*) malloc(n * sizeof(float));
    float* z = (float*) malloc(n * sizeof(float));

    // Solve
    ldlDecomposition(A, L, D, n);
    forwardSubstitution_LDL(L, b, y, n);
    diagonalSolve(D, y, z, n);
    backSubstitution_LDL(L, z, x, n);

    // Free heap
    for (int i = 0; i < n; ++i)
        free(L[i]);
    free(L);
    free(D);
    free(y);
    free(z);
}

#endif
