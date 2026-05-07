#ifndef HESS_LINEAR_IND_H
#define HESS_LINEAR_IND_H

void hess_linear_Ind_x(const float fi[N*4],
                       float hess_fi_Ind[N*dim])
{
  for (int blk = 0; blk < N; blk++) {  // 3 blocks
      int base_idx = 2 + blk * 6;      // 2, 8, 14
      for (int i = 0; i < 4; i++) {    // 4 elements per block
          int idx = base_idx + i;     // index of line/column
          int fi_idx = blk * 4 + i;   // correspondent index in fi[]
          hess_fi_Ind[idx] += 1.0 / (fi[fi_idx]*fi[fi_idx]);
      }
  }

}


void hess_linear_Ind_u(const float fi[N*2],
                       float hess_fi_Ind[N*dim])
{
  for (int blk = 0; blk < N; blk++) {          // 3 blocks
    int base_idx = blk * 6;                    // 0, 6, 12
    for (int i = 0; i < 2; i++) {              // 2 inputs per block
      int idx = base_idx + i;                  // diagonal index in the matrix
      int fi_idx = blk * 2 + i;                // index in fi[]
      float fi_sq = fi[fi_idx] * fi[fi_idx];   // fi^2
      hess_fi_Ind[idx] += 1.0 / fi_sq;
    }
  }
}



#endif /* HESS_LINEAR_IND_H */