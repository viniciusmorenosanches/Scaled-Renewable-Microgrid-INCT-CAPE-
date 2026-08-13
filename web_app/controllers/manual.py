from .base import BaseController

class ManualController(BaseController):
    def __init__(self):
        self.Pc = 0.0
        self.Pd = 0.0
        self.ucc = 0
        self.uel = 0
        self.ref_sol = 0.0
        self.ref_vento = 0.0

    @property
    def name(self) -> str:
        return "Manual Control"

    def update_manual_values(self, Pc: float, Pd: float, ucc: int, uel: int, ref_sol: float = 0.0, ref_vento: float = 0.0):
        """Allows the web server to push slider and toggle inputs from the user."""
        self.Pc = Pc
        self.Pd = Pd
        self.ucc = ucc
        self.uel = uel
        self.ref_sol = ref_sol
        self.ref_vento = ref_vento

    def compute_controls(self, state: dict) -> dict:
        return {
            "Pc": self.Pc,
            "Pd": self.Pd,
            "ucc": self.ucc,
            "uel": self.uel,
            "ref_sol": self.ref_sol,
            "ref_vento": self.ref_vento
        }
