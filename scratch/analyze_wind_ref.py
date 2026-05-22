import scipy.io
import numpy as np

# Load mat files
dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_cenario = scipy.io.loadmat(dir_path + r'\dados_cenario_24h_reformulado.mat')
mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')

pot_wt_cenario = mat_cenario['pot_wt'].flatten()
pot_wt_final = mat_final['Pot_wt'].flatten()

def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

ref_vento = load_csv(dir_path + r'\referenciaVento.csv')
ref_sol = load_csv(dir_path + r'\referenciaSol.csv')

print(f"ref_vento length: {len(ref_vento)}")
print(f"ref_vento min: {ref_vento.min():.4f}, max: {ref_vento.max():.4f}, mean: {ref_vento.mean():.4f}")

# First 150 values of ref_vento
print("\nFirst 120 values of ref_vento:")
print(np.round(ref_vento[:120], 3))

# Let's see: from index 100 onwards, does it align with pot_wt_cenario?
# Let's slice ref_vento from 100 to 2500 (length 2400)
ref_vento_sliced = ref_vento[100:2500]
print(f"\nSliced ref_vento (100:2500) length: {len(ref_vento_sliced)}")

# Let's calculate estimated power using different options:
# 1. Direct from sliced ref_vento without +2.0V offset:
# Pot = (constante_aero * ref_vento_sliced^3) * 50 + 150
# 2. With +2.0V offset:
# Y_ref = ref_vento_sliced + 2.0 (if between 0 and 2)
# Pot = (constante_aero * Y_ref^3) * 50 + 150
# 3. With other scaling factors

constante_aero = 0.5 * 1.225 * np.pi * (0.15**2) * 0.35 # ~0.015152865

y_ref_1 = np.copy(ref_vento_sliced)
pot_est_1 = constante_aero * (y_ref_1**3) * 50 + 150

y_ref_2 = np.copy(ref_vento_sliced)
mask = (y_ref_2 > 0.0) & (y_ref_2 < 2.0)
y_ref_2[mask] += 2.0
pot_est_2 = constante_aero * (y_ref_2**3) * 50 + 150

# Check correlation with pot_wt_cenario
corr_1 = np.corrcoef(pot_wt_cenario, pot_est_1[:len(pot_wt_cenario)])[0, 1]
corr_2 = np.corrcoef(pot_wt_cenario, pot_est_2[:len(pot_wt_cenario)])[0, 1]

print(f"\nCorrelation between pot_wt_cenario and pot_est_1 (no offset): {corr_1:.4f}")
print(f"Correlation between pot_wt_cenario and pot_est_2 (with +2V offset): {corr_2:.4f}")

# Wait, let's see if there is another slice or offset.
# Let's find the correlation between pot_wt_cenario and ref_vento_sliced directly:
corr_direct = np.corrcoef(pot_wt_cenario, ref_vento_sliced[:len(pot_wt_cenario)])[0, 1]
print(f"Correlation between pot_wt_cenario and sliced ref_vento directly: {corr_direct:.4f}")

# Let's print some values of pot_wt_cenario and corresponding ref_vento_sliced
print("\nFirst 10 values after startup (indices 100-110 in ref_vento, 0-10 in pot_wt_cenario):")
print("pot_wt_cenario:", np.round(pot_wt_cenario[:10], 2))
print("ref_vento_sliced:", np.round(ref_vento_sliced[:10], 2))
print("pot_est_2 (with offset):", np.round(pot_est_2[:10], 2))
