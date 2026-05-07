#ifndef GET_U_H
#define GET_U_H

void get_u(float* x, float u[N*2],
            const int nx, const int nu, const int Nu){
  
  for (int k = 0; k < N; k = k + 1){
    int j = 0;
    for (int i = k*nu ; i < (k+1)*nu; i = i + 1){

      u[i] = x[(nx + nu)*k + j];
      
      j = j + 1;
    }
  }

}

#endif /* GET_U_H */