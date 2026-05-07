#ifndef GET_STATE_CONSTRAINT_INFO_H
#define GET_STATE_CONSTRAINT_INFO_H

#include "get_x.h"
#include "get_u.h"
#include "get_error.h"
#include "fi_box_fun.h"

void get_state_constraint_info(float fi_s_min_x0[N*4], float fi_s_max_x0[N*4], float fi_u_min_x0[N*2], float fi_u_max_x0[N*2],
                                float* x, const float state[4], const float reference[4], 
                                int feas, float error[(N-1)*4], float u[2*N], float s_ter[4]){
  const int nx = 4;
  const int nu = 2;
  const int Nx = nx*N;
  const int Nu = nu*N;

  float s[Nx] = {0};
  float s_all[4 + Nx] = {0};

  get_x(x, state, s, s_all, s_ter, nx, nu, Nx);

  get_u(x, u, nx, nu, Nu);

  const int Ny = (N-1)*nx;  // if D == 0
  // y = s[1:Ny]
  get_error(error, reference, s, nx, Ny);

  float x_min[4] = {-100.0, -40.0, 0.0, -40.0};
  float x_max[4] = {100.0, 40.0, 100.0, 40.0};

  feas = 1;

  fi_box_fun_x(fi_s_min_x0, fi_s_max_x0, s, x_min, x_max, nx, feas);


  float u_min[4] = {-40.0, -40.0};
  float u_max[4] = {40.0, 40.0};

  fi_box_fun_u(fi_u_min_x0, fi_u_max_x0, u, u_min, u_max, nu, feas);

}

#endif /* GET_STATE_CONSTRAINT_INFO_H */