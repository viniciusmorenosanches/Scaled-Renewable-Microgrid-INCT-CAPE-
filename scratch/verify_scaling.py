import scipy.io
import numpy as np
import matplotlib.pyplot as plt
import os

# Set paths
dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_final_path = os.path.join(dir_path, 'dados_microrrede_final.mat')
mat_cen_path = os.path.join(dir_path, 'dados_cenario_24h_reformulado.mat')
csv_sol_path = os.path.join(dir_path, 'referenciaSol.csv')
csv_vento_path = os.path.join(dir_path, 'referenciaVento.csv')

output_dir = r'C:\Users\johan\.gemini\antigravity\brain\5ab8927d-19d3-4a39-8387-bfcbf3df09f8'
output_png = os.path.join(output_dir, 'scaling_verification.png')

def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

try:
    # Load data
    mat_final = scipy.io.loadmat(mat_final_path)
    mat_cen = scipy.io.loadmat(mat_cen_path)
    csv_sol = load_csv(csv_sol_path)
    csv_vento = load_csv(csv_vento_path)

    # Extract experimental variables
    # Shift is 51 samples as established by correlation analysis
    shift = 51
    length = 2400
    
    pot_pv_exp = mat_final['Pot_pv'].flatten()[shift:shift+length]
    pot_wt_exp = mat_final['Pot_wt'].flatten()[shift:shift+length]
    
    # In the experimental MAT, values were saved after scaling has been applied.
    # Let's verify PV power scaling: 150 * V_ref
    # Since the closed-loop controller tracks V_ref (referenciaSol),
    # the ideal Pot_pv should be 150 * csv_sol.
    pot_pv_ideal_150 = 150 * csv_sol[:length]
    pot_pv_ideal_100 = 100 * csv_sol[:length] # What it would be with wrong scaling

    # Let's clean up experimental data (apply MATLAB style zero-removal/filtering for presentation)
    for i in range(1, length):
        if pot_pv_exp[i] == 0:
            pot_pv_exp[i] = pot_pv_exp[i-1]
        if pot_wt_exp[i] == 0:
            pot_wt_exp[i] = pot_wt_exp[i-1]

    # Create plots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

    # 1. Solar Power Comparison
    ax1.plot(pot_pv_exp, label='Experimental Pot_pv (from dados_microrrede_final.mat)', color='tab:orange', linewidth=2)
    ax1.plot(pot_pv_ideal_150, label='Ideal PV Power (150 * V_ref)', color='tab:blue', linestyle='--', linewidth=1.5)
    ax1.plot(pot_pv_ideal_100, label='Wrong PV Power (100 * V_ref)', color='tab:red', linestyle=':', linewidth=1.5)
    ax1.set_ylabel('Solar Power (W)', fontsize=12)
    ax1.set_title('Solar Power Verification (150 * V_ref scaling factor)', fontsize=14)
    ax1.grid(True)
    ax1.legend()

    # 2. Wind Power Comparison
    # Wind power is highly dependent on turbine dynamics and speed control.
    # In the simulation scenario, WT power was:
    pot_wt_cen = mat_cen['pot_wt'].flatten()[:length]
    
    ax2.plot(pot_wt_exp, label='Experimental Pot_wt (from dados_microrrede_final.mat)', color='tab:green', linewidth=2)
    ax2.plot(pot_wt_cen, label='Scenario Target WT Power (pot_wt_cen)', color='tab:purple', linestyle='--', linewidth=1.5)
    ax2.set_ylabel('Wind Power (W)', fontsize=12)
    ax2.set_xlabel('Sample Index', fontsize=12)
    ax2.set_title('Wind Power Profile (baseline 180W - 250W)', fontsize=14)
    ax2.grid(True)
    ax2.legend()

    plt.tight_layout()
    plt.savefig(output_png, dpi=150)
    print(f"SUCCESS: Plot saved to {output_png}")
    
    # Calculate Mean Squared Error / Correlation to demonstrate alignment
    corr_pv_150 = np.corrcoef(pot_pv_exp, pot_pv_ideal_150)[0, 1]
    corr_pv_100 = np.corrcoef(pot_pv_exp, pot_pv_ideal_100)[0, 1]
    
    print(f"Correlation between Experimental PV and 150 * V_ref: {corr_pv_150:.4f}")
    print(f"Correlation between Experimental PV and 100 * V_ref: {corr_pv_100:.4f}")
    print(f"Max Experimental PV: {pot_pv_exp.max():.2f} W")
    print(f"Max 150 * V_ref: {pot_pv_ideal_150.max():.2f} W")
    print(f"Max 100 * V_ref: {pot_pv_ideal_100.max():.2f} W")

except Exception as e:
    print(f"ERROR: {e}")
