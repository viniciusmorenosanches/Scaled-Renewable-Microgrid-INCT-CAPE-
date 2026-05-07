#ifndef MPC_CONTROLLER_H
#define MPC_CONTROLLER_H

#include <Arduino.h>

struct ParamsModel {
    const float kx;
    const float ky;
};


class MPCController {
public:
    ParamsModel params_model;
    
    MPCController(ParamsModel model)
    : params_model(model) {
        initialize_mpc();
    }

    // Main function of the MPC
    void compute_command(float* x, const float state[4], const float reference[4]) {
      float rho_1 = -params_model.kx*state[1];
      float rho_2 = -params_model.ky*state[3];
      ip_log_barrier(x, state, reference, rho_1, rho_2);
    }

private:    
    // Init MPC with Apk and Bpk
    void initialize_mpc() {
        
    }

};

#endif
