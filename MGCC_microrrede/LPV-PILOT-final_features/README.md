# LPV-PILOT: Real-time Nonlinear Predictive Control using Interior-point Log-barrier Optimisation Techniques and the LPV Framework

LPV-PILOT is an open-source toolbox for MATLAB and C embedded control applications. The objective of this solver is to solve Nonlinear Modern Predictive Control (NMPC) problems fast (in the **milli** or **micro**-second range).

This toolbox uses the LPV framework to handle nonlinearities and the well-known interior-point log-barrier algorithm, with some particularities optimizing the code for control applications, improving convergence rate while maintaining closed-loop stability certificates.

## Academic Reference & Demonstration

A detailed explanation of the control strategy applied in this solver, as well as a user walk-through, can be found in our paper, available online:
* **Paper:** [https://hal.science/hal-05281686](https://hal.science/hal-05281686)

Moreover, some experimental applications have already been validated using LPV-PILOT, as shown in this autonomous path-following example in a 32-foot sailboat:
* **Video Demo:** [https://www.youtube.com/watch?v=uFlsUjcfmY4](https://www.youtube.com/watch?v=uFlsUjcfmY4)

## How to Use

1.  Clone the `final_features` branch.
2.  Follow the steps presented in "Appendix B" from the paper.

## Contributing
This is the first version of the solver, and it is still being developed. If you have any contributions, please contact us at the following email: `viniciuseislaine@gmail.com`.