import os

try:
    import matlab
    import matlab.engine
    HAS_MATLAB = True
except Exception:
    HAS_MATLAB = False

from .base import BaseController

class MatlabMpcController(BaseController):
    def __init__(self, matlab_dir: str = None):
        self.eng = None
        self.Q_soc = 0.5
        self.Q_h2 = 0.2
        self.Q_load = 0.8
        
        # Find the MGCC_microrrede directory in the repository
        if not matlab_dir:
            base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            self.matlab_dir = os.path.join(base_dir, "MGCC_microrrede")
        else:
            self.matlab_dir = matlab_dir

        if HAS_MATLAB:
            try:
                print(f"[MATLAB] Starting MATLAB engine in background...")
                self.eng = matlab.engine.start_matlab()
                print(f"[MATLAB] Adding path: {self.matlab_dir}")
                self.eng.addpath(self.matlab_dir)
                
                # Add LPV-PILOT subdirectories to MATLAB path recursively
                lpv_path = os.path.join(self.matlab_dir, "LPV-PILOT-final_features")
                self.eng.addpath(lpv_path)
                self.eng.addpath(os.path.join(lpv_path, "init_func"))
                self.eng.addpath(os.path.join(lpv_path, "main_func"))
                self.eng.addpath(os.path.join(lpv_path, "update_func"))
                
                print("[MATLAB] Running microgrid_init_w_demand initialization...")
                self.eng.eval("microgrid_init_w_demand", nargout=0)
                print("[MATLAB] Engine successfully initialized.")
            except Exception as e:
                print(f"[MATLAB] Failed to start MATLAB engine: {e}")
                self.eng = None
        else:
            print("[MATLAB] 'matlab.engine' python package is not installed. MATLAB MPC Controller will run in fallback mode.")

    @property
    def name(self) -> str:
        return "MATLAB MPC Controller"

    def update_weights(self, Q_soc: float, Q_h2: float, Q_load: float):
        self.Q_soc = float(Q_soc)
        self.Q_h2 = float(Q_h2)
        self.Q_load = float(Q_load)
        print(f"[MATLAB] Weights updated to: Q_soc={self.Q_soc}, Q_h2={self.Q_h2}, Q_load={self.Q_load}")

    def compute_controls(self, state: dict) -> dict:
        if not HAS_MATLAB or self.eng is None:
            # Fallback when MATLAB is offline
            return {
                "Pc": 0.0,
                "Pd": 0.0,
                "ucc": 0,
                "uel": 0,
                "warning": "MATLAB Engine Offline"
            }
            
        try:
            # Convert values to standard python floats for MATLAB engine compatibility
            soc_val = float(state.get("SoC", 60.0))
            if soc_val > 1.0:
                soc_val = soc_val / 100.0  # e.g., convert 60.0 to 0.6
            soc_val = max(0.001, min(0.999, soc_val))
                
            h2_val = float(state.get("H2_Level", 0.0)) / 10.0
            h2_val = max(0.01, h2_val)
            
            # Bus total power calculation
            pot_pv = float(state.get("Pot_pv", 0.0))
            pot_wt = float(state.get("Pot_wt", 0.0))
            pot_cc = float(state.get("Pot_cc", 0.0))
            
            # Since uel and ucc are determined by MPC, we approximate current net power
            pot_total_val = (pot_pv + pot_wt + pot_cc) / 1000.0
            demand_val = float(state.get("Demand", 0.0))

            # Push variables to MATLAB workspace
            self.eng.workspace['soc_val'] = soc_val
            self.eng.workspace['h2_val'] = h2_val
            self.eng.workspace['pot_total_val'] = pot_total_val
            self.eng.workspace['pot_wind'] = pot_wt
            self.eng.workspace['pot_solar'] = pot_pv
            self.eng.workspace['demand_val'] = demand_val
            
            # Set weights list in MATLAB double format
            self.eng.workspace['Qe_weights'] = matlab.double([self.Q_soc, self.Q_h2, self.Q_load])
            
            # Update tracking cost and state vectors in MATLAB workspace
            self.eng.eval("mpc = init_mpc_Tracking_cost(mpc, diag(Qe_weights));", nargout=0)
            self.eng.eval("x_prev = [soc_val; h2_val; pot_total_val];", nargout=0)
            self.eng.eval("d = [pot_wind; pot_solar];", nargout=0)
            self.eng.eval("xref = [0.6; 0.3; demand_val / 1000.0];", nargout=0) # 60% SoC ref, 3.0mm H2 ref

            # Solve MPC step
            self.eng.eval("mpc = update_mpc_Aeq(mpc, mpc.Aeq0, Bd0);", nargout=0)
            self.eng.eval("mpc = update_mpc_precomputed_beq(mpc, x_prev, d, n_rho, mpc.beq0, mpc.beq_ff0);", nargout=0)
            self.eng.eval("[u_prev, J, x0] = mpc_solve(x0, x_prev, u_prev, xref, d, mpc, [], [], []);", nargout=0)

            # Retrieve u_prev output
            u_prev = self.eng.workspace['u_prev']
            
            # Map control fraction variables back to system scaling
            Pc = float(u_prev[0][0]) * 400.0
            Pd = float(u_prev[1][0]) * 400.0
            uel = 1 if float(u_prev[2][0]) > 0.5 else 0
            ucc = 1 if float(u_prev[3][0]) > 0.5 else 0

            return {
                "Pc": float(round(Pc, 1)),
                "Pd": float(round(Pd, 1)),
                "ucc": int(ucc),
                "uel": int(uel)
            }
        except Exception as e:
            print(f"[MATLAB] Error executing MPC solver: {e}")
            return {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "error": str(e)}
