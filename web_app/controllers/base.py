from abc import ABC, abstractmethod

class BaseController(ABC):
    @property
    @abstractmethod
    def name(self) -> str:
        """Name of the controller to display in the UI dropdown."""
        pass

    @abstractmethod
    def compute_controls(self, state: dict) -> dict:
        """
        Receives the current state of the microgrid and calculates control actions.
        
        :param state: dict containing:
            - 'SoC': float (0-100)
            - 'H2_Level': float (water level mm)
            - 'Pot_pv': float (W)
            - 'Pot_wt': float (W)
            - 'Pot_cc': float (W)
            - 'Pot_total': float (W)
            - 'ref_sol': float (V)
            - 'ref_vento': float (V)
            
        :return: dict containing:
            - 'Pc': float (W) - battery charge command
            - 'Pd': float (W) - battery discharge command
            - 'ucc': int (0/1) - fuel cell toggle
            - 'uel': int (0/1) - electrolyzer toggle
        """
        pass
