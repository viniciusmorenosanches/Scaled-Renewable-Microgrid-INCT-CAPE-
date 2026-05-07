#ifndef TER_SET_IND_FUN_H
#define TER_SET_IND_FUN_H

void ter_set_Ind_fun(const float x_ref[4], float s_ter[4], float grad_ter[4]) {
  float P[4][4] = {
          {6.5439f, 1.3139f, 0.0000f, 0.0000f},
          {1.3139f, 0.7264f, 0.0000f, 0.0000f},
          {0.0000f, 0.0000f, 6.5443f, 1.3142f},
          {0.0000f, 0.0000f, 1.3142f, 0.7270f}
      };

  float diff[4];
  for (int i = 0; i < 4; ++i)
      diff[i] = x_ref[i] - s_ter[i];

  // grad_ter = P * diff
  for (int i = 0; i < 4; ++i) {
      grad_ter[i] = 0.0f;
      for (int j = 0; j < 4; ++j)
          grad_ter[i] += P[i][j] * diff[j];
  }
  
}

#endif /* TER_SET_IND_FUN_H */