#ifndef GRAD_F0_MPC_H
#define GRAD_F0_MPC_H

void grad_f0_MPC(float grad_f0[N*dim], float error[(N-1)*4], float u[2*N], float grad_ter[4]){
  float gradErrQe[dim*N][(N-1)*4] = {0};

  // First line with 1.0 and 0.1
  gradErrQe[2][0] = 1.0;
  gradErrQe[3][1] = 0.1;

  gradErrQe[4][2] = 1.0;
  gradErrQe[5][3] = 0.1;

  gradErrQe[8][4] = 1.0;
  gradErrQe[9][5] = 0.1;

  gradErrQe[10][6] = 1.0;
  gradErrQe[11][7] = 0.1;

  gradErrQe[14][8] = 1.0;
  gradErrQe[15][9] = 0.1;

  gradErrQe[16][10] = 1.0;
  gradErrQe[17][11] = 0.1;

  gradErrQe[20][12] = 1.0;
  gradErrQe[21][13] = 0.1;

  gradErrQe[22][14] = 1.0;
  gradErrQe[23][15] = 0.1;

  float acc = 0.0;

  for (int i = 0; i < N*dim; i++) {
    acc = 0.0;
    for (int j = 0; j < (N-1)*4; j++) {
        acc += gradErrQe[i][j] * error[j];
    }
    grad_f0[i] -= acc;
  }


  float gradCtlrRu[N*dim][2*N] = {0};

  gradCtlrRu[0][0] = 0.01;
  gradCtlrRu[1][1] = 0.01;

  gradCtlrRu[6][2] = 0.01;
  gradCtlrRu[7][3] = 0.01;

  gradCtlrRu[12][4] = 0.01;
  gradCtlrRu[13][5] = 0.01;

  gradCtlrRu[18][6] = 0.01;
  gradCtlrRu[19][7] = 0.01;

  gradCtlrRu[24][8] = 0.01;
  gradCtlrRu[25][9] = 0.01;

  for (int i = 0; i < N*dim; i++) {
    acc = 0.0;
    for (int j = 0; j < 2*N; j++) {
        acc += gradCtlrRu[i][j] * u[j];
    }
    grad_f0[i] += acc;
  }

  grad_f0[N*dim - 4] -= grad_ter[0]; 
  grad_f0[N*dim - 3] -= grad_ter[1]; 
  grad_f0[N*dim - 2] -= grad_ter[2]; 
  grad_f0[N*dim - 1] -= grad_ter[3]; 
  
}

#endif /* GRAD_F0_MPC_H */