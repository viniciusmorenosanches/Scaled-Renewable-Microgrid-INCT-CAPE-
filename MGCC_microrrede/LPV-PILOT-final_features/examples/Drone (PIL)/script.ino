const int N = 5;
const int dim = 6;
const int m = 30;

int numReceivedFloats = 8; // Quantity of received floats (y, ref_y)
int numSendFloats = 3;     // Quantity of floats to be sent (u)

#include "grad_box_Ind.h"
#include "ip_barrier.h"
#include "MPC_Controller.h"

int v = 0;

float* x0 = (float*) malloc(N * dim * sizeof(float));

ParamsModel params_model = {
    .kx = 0.10,
    .ky = 0.08,
};

MPCController mpc(params_model);

void setup(){
    Serial.begin(115200);
}

void loop(){
  if (Serial.available() >= 4*numReceivedFloats) {
      float receivedFloats[numReceivedFloats];

      // Read the floats sent by MATLAB
      for (int i = 0; i < numReceivedFloats; i++) {
        Serial.readBytes((char*)&receivedFloats[i], sizeof(receivedFloats[i]));
      }
      
      // Read the state variables
      const float state[4] = {receivedFloats[0], receivedFloats[1], receivedFloats[2], receivedFloats[3]};
      
      // Reference
      const float reference[4] = {receivedFloats[4], receivedFloats[5], receivedFloats[6], receivedFloats[7]};

      if (v < 1) {
        x0[0]  = -0.0008;
        x0[1]  = 26.3529;
        x0[2]  = 0.0002;
        x0[3]  = 0.0017;
        x0[4]  = 0.1801;
        x0[5]  = 2.6371;

        x0[6]  = -0.0006;
        x0[7]  = 14.9767;
        x0[8]  = 0.0004;
        x0[9]  = 0.0016;
        x0[10] = 0.4438;
        x0[11] = 4.1305;

        x0[12] = -0.0005;
        x0[13] = 6.0908;
        x0[14] = 0.0005;
        x0[15] = 0.0016;
        x0[16] = 0.8569;
        x0[17] = 4.7330;

        x0[18] = -0.0003;
        x0[19] = 1.2378;
        x0[20] = 0.0007;
        x0[21] = 0.0016;
        x0[22] = 1.3302;
        x0[23] = 4.8492;

        x0[24] = -0.0002;
        x0[25] = -0.4831;
        x0[26] = 0.0008;
        x0[27] = 0.0015;
        x0[28] = 1.8151;
        x0[29] = 4.7931;
        v = 10;
      }

      float start_time = millis();
      mpc.compute_command(x0, state, reference);
      float computing_time = millis() - start_time;

      // Send floats to MATLAB
      float responseFloats[numSendFloats] = {x0[0], x0[1], computing_time};
      for (int i = 0; i < numSendFloats; i++) {
        Serial.write((char*)&responseFloats[i], sizeof(responseFloats[i]));
      }
    
    }

  // if (v < 1) {
  //   /* UNIT TEST */
    
  //   // Fixed references (px, vx, py, vy)
  //   float ref[4] = {5.0, 0.0, 25.0, 0};  

  //   // Fixed states
  //   const float state[4] = {0.0, 0.20, 20.0, 0.20};

  //   const float reference[4] = {5.0, 0.0, 25.0, 0.0};

  //   float* x0 = (float*) malloc(N * dim * sizeof(float));

  //   x0[0]  = -0.0008;
  //   x0[1]  = 26.3529;
  //   x0[2]  = 0.0002;
  //   x0[3]  = 0.0017;
  //   x0[4]  = 0.1801;
  //   x0[5]  = 2.6371;

  //   x0[6]  = -0.0006;
  //   x0[7]  = 14.9767;
  //   x0[8]  = 0.0004;
  //   x0[9]  = 0.0016;
  //   x0[10] = 0.4438;
  //   x0[11] = 4.1305;

  //   x0[12] = -0.0005;
  //   x0[13] = 6.0908;
  //   x0[14] = 0.0005;
  //   x0[15] = 0.0016;
  //   x0[16] = 0.8569;
  //   x0[17] = 4.7330;

  //   x0[18] = -0.0003;
  //   x0[19] = 1.2378;
  //   x0[20] = 0.0007;
  //   x0[21] = 0.0016;
  //   x0[22] = 1.3302;
  //   x0[23] = 4.8492;

  //   x0[24] = -0.0002;
  //   x0[25] = -0.4831;
  //   x0[26] = 0.0008;
  //   x0[27] = 0.0015;
  //   x0[28] = 1.8151;
  //   x0[29] = 4.7931;


  //   float start_time = millis();
  //   //mpc.compute_Aeq(rho_1, rho_2);
  //   //mpc.compute_beq(rho_1, rho_2, state);
  //   mpc.compute_command(x0, state, reference);
  //   float computing_time = millis() - start_time;
  //   Serial.print("Computing time (total): ");
  //   Serial.println(computing_time);

  //   free(x0);

  //   // Serial.print(mpc.params_controller.beq[0], 4);  // 0
  //   // Serial.print(" , ");
  //   // Serial.print(mpc.params_controller.beq[1], 4);  // 0.01
  //   // Serial.print(" , ");
  //   // Serial.print(mpc.params_controller.beq[2], 4);  // -1
  //   // Serial.print(" , ");
  //   // Serial.print(mpc.params_controller.beq[3], 4);  // 0.9980
  //   // Serial.println(" ");
    
  //   /*Serial.print("Command: ");
  //   Serial.print(mpc.command[0]);
  //   Serial.print(" , ");
  //   Serial.print(mpc.command[1]);
  //   Serial.println(" ");

  //   Serial.print("Computing time (total): ");
  //   Serial.println(computing_time);
  //   Serial.print("Computing time (solver): ");
  //   Serial.println(mpc.params_controller.solver_computing_time);
  //   Serial.print("Computing time (LPV Formulation): ");
  //   Serial.println(computing_time - mpc.params_controller.solver_computing_time);*/
  //   v = 10;
  // }
    
}
