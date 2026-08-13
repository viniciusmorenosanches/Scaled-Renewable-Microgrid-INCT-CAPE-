import os
import importlib.util
from .base import BaseController

class CustomController(BaseController):
    def __init__(self):
        self.module_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "custom_uploaded.py")
        self.last_mtime = 0
        self.cached_module = None

    @property
    def name(self) -> str:
        return "Algoritmo Externo (Customizado)"

    def compute_controls(self, state: dict) -> dict:
        fallback = {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "info": "Aguardando upload do script .py"}
        if not os.path.exists(self.module_path):
            return fallback

        try:
            mtime = os.path.getmtime(self.module_path)
            if mtime != self.last_mtime or not self.cached_module:
                spec = importlib.util.spec_from_file_location("custom_uploaded", self.module_path)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                self.cached_module = module
                self.last_mtime = mtime
                print("[Custom Controller] Loaded uploaded script successfully.")

            if hasattr(self.cached_module, "compute_controls"):
                res = self.cached_module.compute_controls(state)
                return {
                    "Pc": float(res.get("Pc", 0.0)),
                    "Pd": float(res.get("Pd", 0.0)),
                    "ucc": int(res.get("ucc", 0)),
                    "uel": int(res.get("uel", 0))
                }
            else:
                return {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "error": "compute_controls(state) function not found"}
        except Exception as e:
            print(f"[Custom Controller] Error running uploaded script: {e}")
            return {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "error": str(e)}
