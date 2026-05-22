import scipy.io
import numpy as np

mat_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede\dados_microrrede_final.mat'
mat = scipy.io.loadmat(mat_path)
pot_wt = mat['Pot_wt'].flatten()
pot_pv = mat['Pot_pv'].flatten()
soc = mat['SoC'].flatten()

print(f"Pot_wt shape: {pot_wt.shape}")
print(f"Pot_wt min: {pot_wt.min():.2f}, max: {pot_wt.max():.2f}, mean: {pot_wt.mean():.2f}")

# Find unique values or get a profile
# Let's print first 100 values and some slices
print("\nFirst 50 values of Pot_wt:")
print(np.round(pot_wt[:50], 2))

# Back-calculate Y_filtrado if Pot_wt = Pot_exata * 50 + 150
# Pot_exata = Pot_wt - 150 / 50
constante_aero = 0.5 * 1.225 * np.pi * (0.15**2) * 0.35 # ~0.015152865
# Pot_wt = Pot_exata * 50 + 150 = constante_aero * Y_filtrado^3 * 50 + 150
# Y_filtrado = ((Pot_wt - 150) / (50 * constante_aero))^(1/3)
y_filt_back = np.zeros_like(pot_wt)
mask = pot_wt > 150
y_filt_back[mask] = ((pot_wt[mask] - 150) / (50 * constante_aero))**(1/3)

print("\nBack-calculated Y_filtrado stats:")
print(f"min: {y_filt_back.min():.2f}, max: {y_filt_back.max():.2f}")
print("First 50 values of back-calculated Y_filtrado:")
print(np.round(y_filt_back[:50], 2))

# Let's inspect the distinct levels in pot_wt
hist, bin_edges = np.histogram(pot_wt, bins=10)
print("\nHistogram of Pot_wt:")
for i in range(len(hist)):
    print(f"Bin {bin_edges[i]:.1f} to {bin_edges[i+1]:.1f}: {hist[i]} samples")
