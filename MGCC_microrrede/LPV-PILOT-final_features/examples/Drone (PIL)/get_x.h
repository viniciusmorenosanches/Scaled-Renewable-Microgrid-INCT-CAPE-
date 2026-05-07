#ifndef GET_X_H
#define GET_X_H

void get_x(float* x, const float state[4], float s[4*N], float s_all[4 + 4*N], float s_ter[4], 
            const int nx, const int nu, const int Nx){
  
  for (int k = 0; k < N; k = k + 1){
    int j = 0;
    for (int i = k*nx ; i < (k+1)*nx; i = i + 1){

      s[i] = x[nx*k + nu*(k+1) + j];
      
      j = j + 1;
    }
  }

  for (int i = 0; i < 4 + Nx; i ++){
    if (i < 4){
      s_all[i] = state[i];
    }
    else{
      s_all[i] = s[i-4];
      if (i >= 4 + Nx - 4){
        s_ter[i - Nx] = s[i - 4];
      }
    }
  }
}

#endif /* GET_X_H */