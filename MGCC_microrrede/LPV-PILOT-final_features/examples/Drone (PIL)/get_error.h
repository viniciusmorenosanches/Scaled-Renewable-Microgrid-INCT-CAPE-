#ifndef GET_ERROR_H
#define GET_ERROR_H

void get_error(float error[(N-1)*4], const float reference[4], float s[4*N],
               const int nx, const int Ny){
  for (int k = 0; k < (N-1); k = k + 1){
    int j = 0;
    for (int i = k*nx ; i < (k+1)*nx; i = i + 1){

      error[i] = reference[j] - s[k*nx + j];
      
      j = j + 1;
    }
  }
}

#endif /* GET_ERROR_H */