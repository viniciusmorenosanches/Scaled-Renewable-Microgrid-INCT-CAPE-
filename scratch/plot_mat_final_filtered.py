import scipy.io
import os
import matplotlib.pyplot as plt
import numpy as np

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_path = os.path.join(dir_path, 'dados_microrrede_final.mat')
output_dir = r'C:\Users\johan\.gemini\antigravity\brain\5ab8927d-19d3-4a39-8387-bfcbf3df09f8'
output_png = os.path.join(output_dir, 'mat_final_filtered_plots.png')

try:
    mat = scipy.io.loadmat(mat_path)
    
    # Extract variables and truncate to 2400 samples
    pot_total = mat['Pot_total'].flatten()[:2400]
    demand = mat['demand'].flatten()[:2400]
    pot_wt = mat['Pot_wt'].flatten()[:2400]
    pot_pv = mat['Pot_pv'].flatten()[:2400]
    soc = mat['SoC'].flatten()[:2400]
    nivel_agua = mat['nivelAgua'].flatten()[:2400]
    
    # MATLAB filtering logic for zeros
    for i in range(1, 2400):
        if pot_total[i] == 0:
            pot_total[i] = pot_total[i-1]
        if demand[i] == 0:
            demand[i] = demand[i-1]
        if pot_wt[i] == 0:
            pot_wt[i] = pot_wt[i-1]
        if pot_pv[i] == 0:
            pot_pv[i] = pot_pv[i-1]
            
    # Treating pot_wt < 50
    for j in range(1, 2400):
        if pot_wt[j] < 50:
            pot_wt[j] = pot_wt[j-1]
            
    # Treating NaNs in nivel_agua if any
    for i in range(1, 2400):
        if np.isnan(nivel_agua[i]):
            nivel_agua[i] = nivel_agua[i-1]
        if np.isnan(soc[i]):
            soc[i] = soc[i-1]
            
    # Treat pot_total < 50
    pot_total = np.maximum(50, pot_total)
    
    # Apply moving average (movmean) to pot_pv
    # MATLAB's movmean with window 100 on a 1D array can be approximated with np.convolve or a sliding window
    window = 100
    pot_pv_filtered = np.copy(pot_pv)
    for i in range(2400):
        start_idx = max(0, i - window // 2)
        end_idx = min(2400, i + window // 2 + 1)
        pot_pv_filtered[i] = np.mean(pot_pv[start_idx:end_idx])
        
    time = np.linspace(0, 24, 2400)
    
    # Plotting exactly like MATLAB: 4 subplots, downsampled by 2
    idx = np.arange(0, 2400, 2)
    
    fig, (ax1, ax2, ax3, ax4) = plt.subplots(4, 1, figsize=(10, 12))
    
    # Subplot 1: Power (W)
    ax1.plot(time[idx], pot_total[idx], label='P_{total}(k)', linewidth=1.5)
    ax1.plot(time[idx], demand[idx], label='P_{demand}(k)', linewidth=1.5)
    ax1.set_ylabel('Power (W)', fontsize=12)
    ax1.set_xlim([0, 24])
    ax1.grid(True)
    ax1.legend(loc='upper right')
    ax1.set_title('Microgrid Performance Profiles')
    
    # Subplot 2: Renewable Energy (W)
    ax2.plot(time[idx], pot_wt[idx], label='P_{wt}(k)', linewidth=2.0)
    ax2.plot(time[idx], pot_pv_filtered[idx], label='P_{pv}(k)', linewidth=2.0)
    ax2.set_ylabel('Renewable\nEnergy (W)', fontsize=12)
    ax2.set_xlim([0, 24])
    ax2.grid(True)
    ax2.legend(loc='upper right')
    
    # Subplot 3: SoC (%)
    ax3.plot(time[idx], soc[idx], label='SoC(k)', color='tab:green', linewidth=2.0)
    ax3.axhline(0.6, color='red', linestyle='--', linewidth=2.0, label='SoC_{ref}(k)')
    ax3.set_ylabel('SoC (%)', fontsize=12)
    ax3.set_xlim([0, 24])
    ax3.grid(True)
    ax3.legend(loc='upper right')
    
    # Subplot 4: Level of H2 (mm)
    ax4.plot(time[idx], nivel_agua[idx], label='H_2', color='tab:purple', linewidth=2.0)
    ax4.axhline(5.0, color='red', linestyle='--', linewidth=2.0, label='H_{2ref}')
    ax4.set_ylabel('Level of H_2\n(mm)', fontsize=12)
    ax4.set_xlabel('Time (h)', fontsize=12)
    ax4.set_xlim([0, 24])
    ax4.grid(True)
    ax4.legend(loc='upper right')
    
    plt.tight_layout()
    plt.savefig(output_png, dpi=150, bbox_inches='tight')
    print("SUCCESS: mat_final_filtered_plots.png saved.")
    
except Exception as e:
    print(f"ERROR: {e}")
