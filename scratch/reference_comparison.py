import scipy.io
import os
import matplotlib.pyplot as plt
import numpy as np

# Paths
dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_path = os.path.join(dir_path, 'dados_cenario_24h_reformulado.mat')
wind_csv = os.path.join(dir_path, 'referenciaVento.csv')
solar_csv = os.path.join(dir_path, 'referenciaSol.csv')
output_dir = r'C:\Users\johan\.gemini\antigravity\brain\5ab8927d-19d3-4a39-8387-bfcbf3df09f8'
output_png = os.path.join(output_dir, 'reference_comparison.png')

def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return values

try:
    mat = scipy.io.loadmat(mat_path)
    mat_pot_pv = mat['pot_pv'].flatten()
    mat_pot_wt = mat['pot_wt'].flatten()
    
    csv_wind = load_csv(wind_csv)
    csv_solar = load_csv(solar_csv)
    
    print(f"MAT pot_pv len: {len(mat_pot_pv)}, CSV solar len: {len(csv_solar)}")
    print(f"MAT pot_wt len: {len(mat_pot_wt)}, CSV wind len: {len(csv_wind)}")
    
    # Let's plot and compare
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
    
    # Solar comparison
    ax1.plot(mat_pot_pv, color='tab:orange', label='MAT pot_pv (W)')
    ax1.set_title('MAT Solar Power Profile')
    ax1.set_ylabel('Power (W)')
    ax1.grid(True)
    ax1.legend()
    
    ax2.plot(csv_solar, color='tab:red', label='CSV referenciaSol (V)')
    ax2.set_title('CSV Solar Reference (Volts)')
    ax2.set_ylabel('Voltage (V)')
    ax2.grid(True)
    ax2.legend()
    
    # Wind comparison
    ax3.plot(mat_pot_wt, color='tab:blue', label='MAT pot_wt (W)')
    ax3.set_title('MAT Wind Power Profile')
    ax3.set_ylabel('Power (W)')
    ax3.grid(True)
    ax3.legend()
    
    # Apply Arduino wind scaling to show what the Arduino would output
    # Equation: Pot_exata = 0.5 * 1.225 * PI * 0.15^2 * Y_filtrado^3 * 0.35
    # Y_filtrado = Y_ref at steady state
    # Y_ref = csv_wind + 2.0 if csv_wind is between 0 and 2
    y_ref_est = []
    for val in csv_wind:
        temp = val
        if temp > 0.0 and temp < 2.0:
            temp += 2.0
        y_ref_est.append(temp)
    
    y_ref_est = np.array(y_ref_est)
    pot_exata_est = 0.5 * 1.225 * np.pi * (0.15**2) * (y_ref_est**3) * 0.35
    
    # Plot both scaling options for comparison
    pot_wt_scaling_original = pot_exata_est * 50 + 150
    pot_wt_scaling_incorrect = pot_exata_est * 10
    
    ax4.plot(csv_wind, color='tab:purple', label='CSV referenciaVento (V)')
    ax4.plot(y_ref_est, color='tab:green', linestyle='--', label='Y_ref (with +2V offset)')
    ax4.set_title('CSV Wind Reference (Volts)')
    ax4.set_ylabel('Voltage (V)')
    ax4.grid(True)
    ax4.legend()
    
    plt.tight_layout()
    plt.savefig(output_png, dpi=150, bbox_inches='tight')
    print("SUCCESS: Comparison plot saved.")
    
    # Print out peak values and positions
    print(f"MAT pot_pv max: {mat_pot_pv.max():.2f} W at index {mat_pot_pv.argmax()}")
    print(f"CSV solar max: {max(csv_solar):.2f} V at index {csv_solar.index(max(csv_solar))}")
    print(f"MAT pot_wt max: {mat_pot_wt.max():.2f} W at index {mat_pot_wt.argmax()}")
    print(f"CSV wind max: {max(csv_wind):.2f} V at index {csv_wind.index(max(csv_wind))}")
    print(f"Original wind scaling max output: {pot_wt_scaling_original.max():.2f} W")
    print(f"Incorrect wind scaling max output: {pot_wt_scaling_incorrect.max():.2f} W")

except Exception as e:
    print(f"ERROR: {e}")
