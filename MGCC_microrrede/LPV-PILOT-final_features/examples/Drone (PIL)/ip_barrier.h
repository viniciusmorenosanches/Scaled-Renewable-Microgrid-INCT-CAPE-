#ifndef IP_BARRIER_H
#define IP_BARRIER_H

#include <Arduino.h>
#include "grad_box_Ind.h"
#include "get_state_constraint_info.h"
#include "grad_f0_MPC.h"
#include "hess_linear_Ind.h"
#include "LDL_Decomposition.h"
#include "compute_Aeq.h"
#include "compute_beq.h"
#include "ter_set_Ind_fun.h"

// Solve min Jk
void ip_log_barrier(float* x, const float state[4], const float reference[4],
                    float rho_1, float rho_2) {
    const float t = 50;
    const float eps = 1e-4;
    const float beta = 0.75;     // beta = 0.5;

    float* fi_s_min_x0 = (float*) malloc(N * 4 * sizeof(float));
    float* fi_s_max_x0 = (float*) malloc(N * 4 * sizeof(float));
    float* fi_u_min_x0 = (float*) malloc(N * 2 * sizeof(float));
    float* fi_u_max_x0 = (float*) malloc(N * 2 * sizeof(float));

    // Allocate before loop
    float* grad_fi_Ind = (float*) calloc(N * dim, sizeof(float));
    float* grad_f0 = (float*) calloc(N * dim, sizeof(float));
    float* grad_J_x0 = (float*) calloc(N * dim, sizeof(float));
    float* hess_fi_Ind = (float*) calloc(N * dim, sizeof(float));
    float* hess_J_x0 = (float*) calloc(N * dim, sizeof(float));
    float* beq = (float*) calloc(N * 4, sizeof(float));
    float* b = (float*) calloc(N*dim + N*4, sizeof(float));
    float* delta_x = (float*) calloc(N*dim + N*4, sizeof(float));
    float* delta_x_prim = (float*) calloc(N*dim, sizeof(float));
    float* xhat = (float*) calloc(N*dim, sizeof(float));

    // Matrix Aeq
    float** Aeq = (float**) malloc(N*4 * sizeof(float*));
    for (int i = 0; i < N*4; i++)
        Aeq[i] = (float*) calloc(N*dim, sizeof(float));

    // Matrix KKT
    float** KKT = (float**) malloc((N*dim + N*4) * sizeof(float*));
    for (int i = 0; i < N*dim + N*4; i++)
        KKT[i] = (float*) calloc((N*dim + N*4), sizeof(float));

    const int Nu = 2*N;
    const int Ny = (N-1)*4;  // if D == 0

    int feas = 1;
    float error[Ny] = {0};
    float u[Nu] = {0};
    float s_ter[4] = {0};
    get_state_constraint_info(fi_s_min_x0, fi_s_max_x0, fi_u_min_x0, fi_u_max_x0, x, state, reference, feas, error, u, s_ter);


    compute_Aeq(rho_1, rho_2, Aeq);
    compute_beq(rho_1, rho_2, state, beq);

    for (int i = 0; i < N*dim; ++i) {
      for (int j = 0; j < N*4; ++j) {
        KKT[i][j + N*dim] = Aeq[j][i];  // Transpose
      }
    }
    for (int i = 0; i < N*4; ++i) {
      for (int j = 0; j < N*dim; ++j) {
          KKT[i + N*dim][j] = Aeq[i][j];
      }
    }

    float lambda2 = 1.0f;
    while (eps <= lambda2*0.5){     
      memset(grad_fi_Ind, 0, N*dim*sizeof(float));
      memset(grad_f0, 0, N*dim*sizeof(float));
      memset(grad_J_x0, 0, N*dim*sizeof(float));
      memset(hess_fi_Ind, 0, N*dim*sizeof(float));
      memset(hess_J_x0, 0, N*dim*sizeof(float));
      //memset(beq, 0, N*4*sizeof(float));
      memset(b, 0, (N*dim+N*4)*sizeof(float));
      memset(delta_x, 0, (N*dim+N*4)*sizeof(float));
      memset(delta_x_prim, 0, N*dim*sizeof(float));
      memset(xhat, 0, N*dim*sizeof(float));

      const int upper = 1;
      const int lower = 0;

      grad_box_Ind_xbound(fi_s_min_x0, lower, grad_fi_Ind);

      grad_box_Ind_xbound(fi_s_max_x0, upper, grad_fi_Ind);

      grad_box_Ind_ubound(fi_u_min_x0, lower, grad_fi_Ind);

      grad_box_Ind_ubound(fi_u_max_x0, upper, grad_fi_Ind);

      float grad_ter[4] = {0};

      ter_set_Ind_fun(reference, s_ter, grad_ter);

      grad_f0_MPC(grad_f0, error, u, grad_ter);

      for (int i = 0; i < N*dim; i++) {
        grad_J_x0[i] = t*grad_f0[i] + grad_fi_Ind[i];
      }


      hess_linear_Ind_x(fi_s_min_x0, hess_fi_Ind);

      // The hessXmax is the same in this case

      hess_linear_Ind_x(fi_s_max_x0, hess_fi_Ind);

      hess_linear_Ind_u(fi_u_min_x0, hess_fi_Ind);

      hess_linear_Ind_u(fi_u_max_x0, hess_fi_Ind);

      for (int blk = 0; blk < N; blk++) {
          int base = blk * dim;
          hess_J_x0[base] = t*0.01 + hess_fi_Ind[base];
          hess_J_x0[base + 1] = t*0.01 + hess_fi_Ind[base + 1];
          hess_J_x0[base + 2] = hess_fi_Ind[base + 2];
          hess_J_x0[base + 3] = hess_fi_Ind[base + 3];
          hess_J_x0[base + 4] = hess_fi_Ind[base + 4];
          hess_J_x0[base + 5] = hess_fi_Ind[base + 5];
          if (blk < (N-1)){
            hess_J_x0[base + 2] += t*1.0;
            hess_J_x0[base + 3] += t*0.1;
            hess_J_x0[base + 4] += t*1.0;
            hess_J_x0[base + 5] += t*0.1;
          }
          
      }

      for (int i = 0; i < N*dim; ++i) {
        KKT[i][i] = hess_J_x0[i];
      }
      float P[4][4] = {
          {6.5439f, 1.3139f, 0.0000f, 0.0000f},
          {1.3139f, 0.7264f, 0.0000f, 0.0000f},
          {0.0000f, 0.0000f, 6.5443f, 1.3142f},
          {0.0000f, 0.0000f, 1.3142f, 0.7270f}
      };
      KKT[N*dim-4][N*dim-4] = t*P[0][0];
      KKT[N*dim-4][N*dim-3] = t*P[0][1];
      KKT[N*dim-4][N*dim-2] = t*P[0][2];
      KKT[N*dim-4][N*dim-1] = t*P[0][3];

      KKT[N*dim-3][N*dim-4] = t*P[1][0];
      KKT[N*dim-3][N*dim-3] = t*P[1][1];
      KKT[N*dim-3][N*dim-2] = t*P[1][2];
      KKT[N*dim-3][N*dim-1] = t*P[1][3];

      KKT[N*dim-2][N*dim-4] = t*P[2][0];
      KKT[N*dim-2][N*dim-3] = t*P[2][1];
      KKT[N*dim-2][N*dim-2] = t*P[2][2];
      KKT[N*dim-2][N*dim-1] = t*P[2][3];

      KKT[N*dim-1][N*dim-4] = t*P[3][0];
      KKT[N*dim-1][N*dim-3] = t*P[3][1];
      KKT[N*dim-1][N*dim-2] = t*P[3][2];
      KKT[N*dim-1][N*dim-1] = t*P[3][3];


      

      for (int i = 0; i < N*dim; ++i) {
          b[i] = grad_J_x0[i];
      }

      // Part 2: Aeq * x - beq
      for (int i = 0; i < N*4; ++i) {
          float acc = 0.0;
          for (int j = 0; j < N*dim; ++j) {
              acc += Aeq[i][j] * x[j];
          }
          b[i + N*dim] = acc - beq[i];
      }

      const int size_matrix = 50;
      solveSystemLDL(KKT, b, delta_x, size_matrix);

      for (int i = 0; i < (N*dim); i++) {
        delta_x_prim[i] = - delta_x[i];
      }

      float compute_lambda2 = 0.0f;
      for (int i = 0; i < (N*dim); ++i) {
          compute_lambda2 -= grad_J_x0[i] * delta_x_prim[i];
      }
      lambda2 = compute_lambda2;

      // FEASIBILITY LINE SEARCH
      float l = 1.0;
      //float xhat[N*dim] = {0};

      for (int i = 0; i < (N*dim); i++){
        xhat[i] = x[i] + l*delta_x_prim[i];
      }

      get_state_constraint_info(fi_s_min_x0, fi_s_max_x0, fi_u_min_x0, fi_u_max_x0, xhat, state, reference, feas, error, u, s_ter);

      if (feas){
        for (int i = 0; i < (N*dim); i++){
          x[i] = xhat[i];
        }
      }
      else{
        while (!feas){

          l = l * beta;
          for (int i = 0; i < (N*dim); i++){
            xhat[i] = x[i] + l*delta_x_prim[i];
          }
          get_state_constraint_info(fi_s_min_x0, fi_s_max_x0, fi_u_min_x0, fi_u_max_x0, xhat, state, reference, feas, error, u, s_ter);

        }

        for (int i = 0; i < (N*dim); i++){
          x[i] = xhat[i];
        }
      }
    }
    free(fi_s_min_x0);
    free(fi_s_max_x0);
    free(fi_u_min_x0);
    free(fi_u_max_x0);

    for (int i = 0; i < N*4; i++) free(Aeq[i]);
    free(Aeq);

    for (int i = 0; i < (N*dim + N*4); i++) free(KKT[i]);
    free(KKT);

    free(grad_fi_Ind);
    free(grad_f0);
    free(grad_J_x0);
    free(hess_fi_Ind);
    free(hess_J_x0);
    free(beq);
    free(b);
    free(delta_x);
    free(delta_x_prim);
    free(xhat);  
}


#endif
