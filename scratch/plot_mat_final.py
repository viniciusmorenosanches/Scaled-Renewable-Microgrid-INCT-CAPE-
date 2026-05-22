import scipy.io
import os
import matplotlib.pyplot as plt
import numpy as np

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_path = os.path.join(dir_path, 'dados_microrrede_final.mat')
output_dir = r'C:\Users\johan\.gemini\antigravity\brain\5ab8927d-19d3-4a39-8387-bfcbf3df09f8'
output_png = os.path.join(output_dir, 'mat_final_plots.png')

try:
    mat = scipy.io.loadmat(mat_path)
    
    # Extract variables
    # Note that variables might have different lengths or shapes, let's flatten them
    pot_pv = mat['Pot_pv'].flatten()
    pot_wt = mat['Pot_wt'].flatten()
    soc = mat['SoC'].flatten()
    nivel_agua = mat['nivelAgua'].flatten()
    
    print(f"Pot_pv len: {len(pot_pv)}")
    print(f"Pot_wt len: {len(pot_wt)}")
    print(f"SoC len: {len(soc)}")
    print(f"nivelAgua len: {len(nivel_agua)}")
    
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
    
    ax1.plot(pot_pv, color='tab:orange', label='Pot_pv (W)')
    ax1.set_title('Pot_pv (PV Power)')
    ax1.set_ylabel('Power (W)')
    ax1.grid(True)
    ax1.legend()
    
    ax2.plot(pot_wt, color='tab:blue', label='Pot_wt (W)')
    ax2.set_title('Pot_wt (Wind Power)')
    ax2.set_ylabel('Power (W)')
    ax2.grid(True)
    ax2.legend()
    
    ax3.plot(soc, color='tab:green', label='SoC')
    ax3.axhline(0.6, color='red', linestyle='--', label='SoC ref (0.6)')
    ax3.set_title('State of Charge (SoC)')
    ax3.set_ylabel('SoC (%)')
    ax3.grid(True)
    ax3.legend()
    
    # Replace NaN in level with forward fill/backward fill for display if needed
    # but first let's see if there are any non-nan values
    valid_mask = ~np.isnan(nivel_agua)
    if np.any(valid_mask):
        ax4.plot(nivel_agua, color='tab:purple', label='nivelAgua (mm)')
    else:
        ax4.text(0.5, 0.5, 'nivelAgua is all NaN', ha='center', va='center')
    ax4.axhline(5.0, color='red', linestyle='--', label='H2 ref (5 mm)')
    ax4.set_title('Hydrogen Level (nivelAgua)')
    ax4.set_ylabel('Level (mm)')
    ax4.grid(True)
    ax4.legend()
    
    plt.tight_layout()
    plt.savefig(output_png, dpi=150, bbox_inches='tight')
    print("SUCCESS: mat_final_plots.png saved.")
    
except Exception as e:
    print(f"ERROR: {e}")
