#ifndef COMPUTE_BEQ_H
#define COMPUTE_BEQ_H

void compute_beq(float rho_1, float rho_2, const float state[4], float* beq) {
      beq[0] = -1.0*state[0] + (-0.1)*state[1];
      beq[1] = (-1.0 - 0.1*rho_1)*state[1];
      beq[2] = -1.0*state[2] + (-0.1)*state[3];
      beq[3] = (-1.0 - 0.1*rho_2)*state[3];
    }

#endif /* COMPUTE_AEQ_H */