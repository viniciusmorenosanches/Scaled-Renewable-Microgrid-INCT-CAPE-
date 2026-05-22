import scipy.io
import numpy as np
import matplotlib.pyplot as plt

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_cenario = scipy.io.loadmat(dir_path + r'\dados_cenario_24h_reformulado.mat')
mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')

pot_wt_cenario = mat_cenario['pot_wt'].flatten()
pot_wt_final = mat_final['Pot_wt'].flatten()

constante_aero = 0.5 * 1.225 * np.pi * (0.15**2) * 0.35 # ~0.01515286523
factor_original = 50.0

def back_calc_y_ref(pot, factor, offset_subtraction=True):
    # Pot = Pot_exata * factor + 150
    # Pot_exata = (Pot - 150) / factor
    # Y_ref = (Pot_exata / constante_aero) ** (1/3)
    pot_exata = (pot - 150.0) / factor
    # handle negative values
    pot_exata = np.maximum(0, pot_exata)
    y_ref = (pot_exata / constante_aero) ** (1/3)
    
    # In the Arduino code:
    # if (tempRef > 0.0 && tempRef < 2.0) { tempRef = tempRef + 2.0; }
    # So if Y_ref is between 2.0 and 4.0, the original tempRef was Y_ref - 2.0.
    # If Y_ref is outside, or if we want the actual Y_ref sent by MATLAB:
    tempRef = np.copy(y_ref)
    if offset_subtraction:
        mask = (y_ref > 2.0) & (y_ref < 4.0)
        tempRef[mask] -= 2.0
    return y_ref, tempRef

y_ref_cen, tempRef_cen = back_calc_y_ref(pot_wt_cenario, factor_original)
y_ref_fin, tempRef_fin = back_calc_y_ref(pot_wt_final, factor_original)

print("Scenario Pot_wt stats:")
print(f"min: {pot_wt_cenario.min():.2f}, max: {pot_wt_cenario.max():.2f}, mean: {pot_wt_cenario.mean():.2f}")
print("Scenario back-calculated Y_ref stats:")
print(f"min: {y_ref_cen.min():.2f}, max: {y_ref_cen.max():.2f}, mean: {y_ref_cen.mean():.2f}")
print("Scenario back-calculated CSV tempRef stats:")
print(f"min: {tempRef_cen.min():.2f}, max: {tempRef_cen.max():.2f}, mean: {tempRef_cen.mean():.2f}")

print("\nFinal Exp Pot_wt stats:")
print(f"min: {pot_wt_final.min():.2f}, max: {pot_wt_final.max():.2f}, mean: {pot_wt_final.mean():.2f}")
print("Final Exp back-calculated Y_ref stats (first 100 values are startup 0s):")
non_zero_fin = pot_wt_final > 150.1
y_ref_fin_nz = y_ref_fin[non_zero_fin]
print(f"min: {y_ref_fin_nz.min():.2f}, max: {y_ref_fin_nz.max():.2f}, mean: {y_ref_fin_nz.mean():.2f}")

# Let's check the correlation between the back-calculated tempRef_cen (which represents the reference sent by MATLAB in simulation)
# and the loaded csv referenciaVento.csv.
# Remember, cenario has 2400 samples. Let's see if we can find any lag or subset of csv that matches it.
with open(dir_path + r'\referenciaVento.csv', 'r') as f:
    csv_wind = np.array([float(x.strip()) for x in f.read().replace('\n', ',').replace('\r', ',').split(',') if x.strip()])

print(f"\ncsv_wind length: {len(csv_wind)}")

# Cross-correlation to see if there is a lag
from scipy.signal import correlate
# Let's normalize both for cross correlation
a = (tempRef_cen - np.mean(tempRef_cen)) / (np.std(tempRef_cen) * len(tempRef_cen))
b = (csv_wind - np.mean(csv_wind)) / np.std(csv_wind)
corr = correlate(b, a, mode='valid')
best_lag = np.argmax(corr)
print(f"Best lag: {best_lag}, Max correlation: {corr[best_lag]:.4f}")

# Let's check if the CSV matches when shifted by best_lag
if best_lag < len(csv_wind) - len(tempRef_cen):
    csv_slice = csv_wind[best_lag:best_lag + len(tempRef_cen)]
    corr_val = np.corrcoef(tempRef_cen, csv_slice)[0, 1]
    print(f"Correlation at lag {best_lag}: {corr_val:.4f}")
    
# Let's save a plot to see what is going on
plt.figure(figsize=(10, 6))
plt.plot(tempRef_cen, label='Back-calculated tempRef (from cenario pot_wt)', alpha=0.8)
if best_lag < len(csv_wind) - len(tempRef_cen):
    plt.plot(csv_wind[best_lag:best_lag + len(tempRef_cen)], label=f'CSV reference shifted by {best_lag}', alpha=0.8)
else:
    plt.plot(csv_wind[:len(tempRef_cen)], label='CSV reference (no shift)', alpha=0.8)
plt.legend()
plt.title('Comparison of back-calculated wind reference vs CSV')
plt.grid(True)
plt.savefig(r'C:\Users\johan\.gemini\antigravity\brain\5ab8927d-19d3-4a39-8387-bfcbf3df09f8\wind_ref_backcalc_vs_csv.png')
print("Plot saved to wind_ref_backcalc_vs_csv.png")
