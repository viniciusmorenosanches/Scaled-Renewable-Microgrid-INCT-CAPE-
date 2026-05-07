#ifndef COMPUTE_GRAD_HESSIAN_H
#define COMPUTE_GRAD_HESSIAN_H

#include "grad_box_Ind.h"
#include "get_state_constraint_info.h"
#include "grad_f0_MPC.h"
#include "hess_linear_Ind.h"

void compute_grad_hessian(float* x, const float state[4], const float reference[4],
                          float Aeq[N*4][N*2], float beq[N*4], const float t,
                          float grad_J_x0[N*dim], float hess_J_x0[N*dim])
{
    float fi_s_min_x0[N*4] = {0};
    float fi_s_max_x0[N*4] = {0};
    float fi_u_min_x0[N*2] = {0};
    float fi_u_max_x0[N*2] = {0};

    const int Nu = 2*N;
    const int Ny = (N-1)*4;  // if D == 0

    int feas = 1;
    float error[Ny] = {0};
    float u[Nu] = {0};
    get_state_constraint_info(fi_s_min_x0, fi_s_max_x0, fi_u_min_x0, fi_u_max_x0, x, state, reference, feas, error, u);

    const int upper = 1;
    const int lower = 0;

    float grad_fi_Ind[N*dim] = {0};

    grad_box_Ind_xbound(fi_s_min_x0, lower, grad_fi_Ind);

    grad_box_Ind_xbound(fi_s_max_x0, upper, grad_fi_Ind);

    grad_box_Ind_ubound(fi_u_min_x0, lower, grad_fi_Ind);

    grad_box_Ind_ubound(fi_u_max_x0, upper, grad_fi_Ind);

    float grad_f0[N*dim] = {0};
    grad_f0_MPC(grad_f0, error, u);

    for (int i = 0; i < N*dim; i++) {
      grad_J_x0[i] = t*grad_f0[i] + grad_fi_Ind[i];
    }

    float hess_fi_Ind[N*dim] = {0};

    hess_linear_Ind_x(fi_s_min_x0, hess_fi_Ind);

    // The hessXmax is the same in this case

    hess_linear_Ind_x(fi_s_max_x0, hess_fi_Ind);

    hess_linear_Ind_u(fi_u_min_x0, hess_fi_Ind);

    hess_linear_Ind_u(fi_u_max_x0, hess_fi_Ind);

    for (int blk = 0; blk < 3; blk++) {
        int base = blk * 6;
        hess_J_x0[base] = t*0.01 + hess_fi_Ind[base];
        hess_J_x0[base + 1] = t*0.01 + hess_fi_Ind[base + 1];
        hess_J_x0[base + 2] = hess_fi_Ind[base + 2];
        hess_J_x0[base + 3] = hess_fi_Ind[base + 3];
        hess_J_x0[base + 4] = hess_fi_Ind[base + 4];
        hess_J_x0[base + 5] = hess_fi_Ind[base + 5];
        if (blk < 2){
          hess_J_x0[base + 2] += t*1.0;
          hess_J_x0[base + 3] += t*0.1;
          hess_J_x0[base + 4] += t*1.0;
          hess_J_x0[base + 5] += t*0.1;
        }
        
    }
}



#endif /* COMPUTE_GRAD_HESSIAN_H */