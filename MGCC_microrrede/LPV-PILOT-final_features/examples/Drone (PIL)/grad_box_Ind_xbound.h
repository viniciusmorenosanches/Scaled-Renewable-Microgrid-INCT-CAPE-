#ifndef GRAD_BOX_IND_H
#define GRAD_BOX_IND_H

void grad_box_Ind_xbound(const float fi[N*4],
                         const int bound, //upper = 1, lower = 0
                         float grad_Ind_x0[N*dim])
{
    const int n = N*dim;
    const int m = N*4;

    float grad_fi[N*dim][N*4] = {0};

    if (bound == 0){
      grad_fi[2][0]  = -1.0;
      grad_fi[3][1]  = -1.0;
      grad_fi[4][2]  = -1.0;
      grad_fi[5][3]  = -1.0;

      grad_fi[8][4]  = -1.0;
      grad_fi[9][5]  = -1.0;
      grad_fi[10][6] = -1.0;
      grad_fi[11][7] = -1.0;

      grad_fi[14][8]  = -1.0;
      grad_fi[15][9]  = -1.0;
      grad_fi[16][10] = -1.0;
      grad_fi[17][11] = -1.0;

      grad_fi[20][12]  = -1.0;
      grad_fi[21][13]  = -1.0;
      grad_fi[22][14] = -1.0;
      grad_fi[23][15] = -1.0;

      grad_fi[26][16]  = -1.0;
      grad_fi[27][17]  = -1.0;
      grad_fi[28][18] = -1.0;
      grad_fi[29][19] = -1.0;
    }
    else{
      grad_fi[2][0]  = 1.0;
      grad_fi[3][1]  = 1.0;
      grad_fi[4][2]  = 1.0;
      grad_fi[5][3]  = 1.0;

      grad_fi[8][4]  = 1.0;
      grad_fi[9][5]  = 1.0;
      grad_fi[10][6] = 1.0;
      grad_fi[11][7] = 1.0;

      grad_fi[14][8]  = 1.0;
      grad_fi[15][9]  = 1.0;
      grad_fi[16][10] = 1.0;
      grad_fi[17][11] = 1.0;

      grad_fi[20][12]  = 1.0;
      grad_fi[21][13]  = 1.0;
      grad_fi[22][14] = 1.0;
      grad_fi[23][15] = 1.0;

      grad_fi[26][16]  = 1.0;
      grad_fi[27][17]  = 1.0;
      grad_fi[28][18] = 1.0;
      grad_fi[29][19] = 1.0;
    }
    
        
    /* sum -grad_fi(:,i)/fi(i) */
    for (int i = 0; i < m; ++i)
    {
        /* simple protection against division by ~0 */
        // if (fi[c] == 0.0) {
        //   continue;
        // }

        for (int r = 0; r < n; ++r)
          grad_Ind_x0[r] -= grad_fi[r][i] / fi[i];
    }
}

void grad_box_Ind_ubound(const float fi[N*2],
                         const int bound, // upper: 1, lower: 0
                         float grad_Ind_x0[N*dim])
{
    const int n = N*dim;
    const int m = N*2;

    float grad_fi[N*dim][N*2] = {0};

    if (bound == 0){
      grad_fi[0][0]  = -1.0;
      grad_fi[1][1]  = -1.0;

      grad_fi[6][2]  = -1.0;
      grad_fi[7][3]  = -1.0;

      grad_fi[12][4] = -1.0;
      grad_fi[13][5] = -1.0;

      grad_fi[18][6] = -1.0;
      grad_fi[19][7] = -1.0;

      grad_fi[24][8] = -1.0;
      grad_fi[25][9] = -1.0;
    }
    else{
      grad_fi[0][0]  = 1.0;
      grad_fi[1][1]  = 1.0;

      grad_fi[6][2]  = 1.0;
      grad_fi[7][3]  = 1.0;

      grad_fi[12][4] = 1.0;
      grad_fi[13][5] = 1.0;

      grad_fi[18][6] = 1.0;
      grad_fi[19][7] = 1.0;

      grad_fi[24][8] = 1.0;
      grad_fi[25][9] = 1.0;
    }
        
    /* sum -grad_fi(:,i)/fi(i) */
    for (int i = 0; i < m; ++i)
    {
        /* simple protection against division by ~0 */
        // if (fi[c] == 0.0) {
        //   continue;
        // }

        for (int r = 0; r < n; ++r)
            grad_Ind_x0[r] -= grad_fi[r][i] / fi[i];
    }
}

#endif /* GRAD_BOX_IND_H */