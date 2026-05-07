#ifndef FI_BOX_FUN_H
#define FI_BOX_FUN_H

void fi_box_fun_x(float fi_s_min_x0[N*4], float fi_s_max_x0[N*4], float s[4*N], float x_min[4], float x_max[4],
                  const int nx, int feas){

  
  for (int k = 0; k < N; k = k + 1){
    int j = 0;
    for (int i = k*nx ; i < (k+1)*nx; i = i + 1){
      fi_s_min_x0[i] = x_min[j] - s[k*nx + j];
      fi_s_max_x0[i] = s[k*nx + j] - x_max[j];

      if (fi_s_min_x0[i] > 0 || fi_s_max_x0[i] > 0){
        feas = 0;
      }
      
      j = j + 1;
    }
  }
}

void fi_box_fun_u(float fi_u_min_x0[N*2], float fi_u_max_x0[N*2], float u[2*N], float u_min[2], float u_max[2],
                  const int nu, int feas){

  
  for (int k = 0; k < N; k = k + 1){
    int j = 0;
    for (int i = k*nu ; i < (k+1)*nu; i = i + 1){
      fi_u_min_x0[i] = u_min[j] - u[k*nu + j];
      fi_u_max_x0[i] = u[k*nu + j] - u_max[j];

      if (fi_u_min_x0[i] > 0 || fi_u_max_x0[i] > 0){
        feas = 0;
      }
      
      j = j + 1;
    }
  }
}

#endif /* FI_BOX_FUN_H */