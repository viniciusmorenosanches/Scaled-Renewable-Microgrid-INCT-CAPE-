from .base import BaseController

class HeuristicController(BaseController):
    def __init__(self):
        # State variables for PI controller
        self.integrator = 0.0
        self.SoC_ref = 0.6  # 60% desired SoC reference
        self.Kp = 150.0
        self.Ki = 10.0
        self.Ts = 0.1
        self.Pc_max = 200.0
        self.Pd_max = 200.0

    @property
    def name(self) -> str:
        return "Automatic (PI SoC + H2 Rules)"

    def compute_controls(self, state: dict) -> dict:
        # 1. H2 Level Control (Hysteresis rule)
        h2_level = state.get("H2_Level", 0.0)
        # Note: level starts at 0.0. Level ranges.
        # If level >= 3.0, enable Fuel Cell (ucc) and disable Electrolyzer (uel)
        # If level <= 2.5, disable Fuel Cell (ucc) and enable Electrolyzer (uel)
        if h2_level >= 3.0:
            ucc = 1
            uel = 0
        elif h2_level <= 2.5:
            ucc = 0
            uel = 1
        else:
            # Maintain previous values or default to 0 if not initialized
            # To keep it simple and robust, default to 0
            ucc = 0
            uel = 0

        # 2. PI Controller for SoC
        # SoC comes from Arduino as a percentage (0-100 or 0-1)
        # In MATLAB, SoC is a fraction (e.g. 0.6 is 60%). Let's check state['SoC'].
        # ArduinoCentral sends SoC. Let's make sure we convert percentage to fraction if it is > 1.0.
        soc_val = state.get("SoC", 60.0)
        if soc_val > 1.0:
            soc_val = soc_val / 100.0  # Convert e.g., 60.0 to 0.6
            
        error = self.SoC_ref - soc_val
        self.integrator += error * self.Ts
        
        # Control output
        u = (self.Kp * error) + (self.Ki * self.integrator)
        
        # Distribute control signal to Charge (Pc) or Discharge (Pd)
        if u > 0:
            Pc = min(u, self.Pc_max)
            Pd = 0.0
        else:
            Pc = 0.0
            Pd = min(-u, self.Pd_max)
            
        return {
            "Pc": float(round(Pc, 1)),
            "Pd": float(round(Pd, 1)),
            "ucc": int(ucc),
            "uel": int(uel)
        }
