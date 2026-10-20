#include "MatrixMath.h"
#include <math.h>

MatrixMath::MatrixMath() {
}

void MatrixMath::Print(mtx_type* A, int m, int n, String label) {
    Serial.println();
    Serial.println(label);
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            Serial.print(A[n * i + j]);
            Serial.print("\t");
        }
        Serial.println();
    }
}

void MatrixMath::Copy(mtx_type* A, int n, int m, mtx_type* B) {
    int total = n * m;
    for (int i = 0; i < total; i++) {
        B[i] = A[i];
    }
}

void MatrixMath::Multiply(mtx_type* A, mtx_type* B, int m, int p, int n, mtx_type* C) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            C[n * i + j] = 0;
            for (int k = 0; k < p; k++) {
                C[n * i + j] += A[p * i + k] * B[n * k + j];
            }
        }
    }
}

void MatrixMath::Add(mtx_type* A, mtx_type* B, int m, int n, mtx_type* C) {
    int total = m * n;
    for (int i = 0; i < total; i++) {
        C[i] = A[i] + B[i];
    }
}

void MatrixMath::Subtract(mtx_type* A, mtx_type* B, int m, int n, mtx_type* C) {
    int total = m * n;
    for (int i = 0; i < total; i++) {
        C[i] = A[i] - B[i];
    }
}

void MatrixMath::Transpose(mtx_type* A, int m, int n, mtx_type* C) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            C[m * j + i] = A[n * i + j];
        }
    }
}

void MatrixMath::Scale(mtx_type* A, int m, int n, mtx_type k) {
    int total = m * n;
    for (int i = 0; i < total; i++) {
        A[i] = A[i] * k;
    }
}

int MatrixMath::Invert(mtx_type* A, int n) {
    if (n == 2) {
        mtx_type det = A[0] * A[3] - A[1] * A[2];
        if (fabs(det) < 1e-9) return 0;
        mtx_type invdet = 1.0f / det;
        mtx_type a00 = A[0], a01 = A[1], a10 = A[2], a11 = A[3];
        A[0] =  a11 * invdet;
        A[1] = -a01 * invdet;
        A[2] = -a10 * invdet;
        A[3] =  a00 * invdet;
        return 1;
    }

    mtx_type A_aug[4][8];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A_aug[i][j] = A[n * i + j];
            A_aug[i][j + n] = (i == j) ? 1.0f : 0.0f;
        }
    }

    for (int i = 0; i < n; i++) {
        mtx_type pivot = A_aug[i][i];
        if (fabs(pivot) < 1e-9) {
            int swap_row = -1;
            for (int k = i + 1; k < n; k++) {
                if (fabs(A_aug[k][i]) > 1e-9) {
                    swap_row = k;
                    break;
                }
            }
            if (swap_row == -1) return 0;
            for (int j = 0; j < 2 * n; j++) {
                mtx_type tmp = A_aug[i][j];
                A_aug[i][j] = A_aug[swap_row][j];
                A_aug[swap_row][j] = tmp;
            }
            pivot = A_aug[i][i];
        }

        for (int j = 0; j < 2 * n; j++) {
            A_aug[i][j] /= pivot;
        }

        for (int k = 0; k < n; k++) {
            if (k != i) {
                mtx_type factor = A_aug[k][i];
                for (int j = 0; j < 2 * n; j++) {
                    A_aug[k][j] -= factor * A_aug[i][j];
                }
            }
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            A[n * i + j] = A_aug[i][j + n];
        }
    }

    return 1;
}

MatrixMath Matrix;
