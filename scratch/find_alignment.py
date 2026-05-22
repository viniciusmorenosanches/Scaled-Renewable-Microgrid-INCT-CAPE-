import scipy.io
import numpy as np
import matplotlib.pyplot as plt

# Load CSV files
def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
csv_sol = load_csv(dir_path + r'\referenciaSol.csv')
csv_vento = load_csv(dir_path + r'\referenciaVento.csv')

# Load final experimental data
mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')
pot_pv_exp = mat_final['Pot_pv'].flatten()
pot_wt_exp = mat_final['Pot_wt'].flatten()

print(f"csv_sol length: {len(csv_sol)}, csv_vento length: {len(csv_vento)}")
print(f"pot_pv_exp length: {len(pot_pv_exp)}, pot_wt_exp length: {len(pot_wt_exp)}")

# Find best correlation of csv_sol with pot_pv_exp
# Since csv_sol is 2400, and pot_pv_exp is 2910, let's try different shifts
best_sol_corr = -1
best_sol_shift = -1
for shift in range(0, len(pot_pv_exp) - len(csv_sol) + 1):
    pot_pv_slice = pot_pv_exp[shift:shift+len(csv_sol)]
    corr = np.corrcoef(csv_sol, pot_pv_slice)[0, 1]
    if corr > best_sol_corr:
        best_sol_corr = corr
        best_sol_shift = shift

print(f"\nPV Solar Reference Alignment:")
print(f"Best correlation: {best_sol_corr:.4f} at shift {best_sol_shift}")

# Let's back-calculate the actual wind reference voltage used in the experiment:
# Pot_wt = Pot_exata * 50 + 150 ? No, wait, in the experimental data, was it scaling by 50+150 or by 10?
# Let's check the stats of pot_wt_exp
print(f"\npot_wt_exp stats: min={pot_wt_exp.min():.2f}, max={pot_wt_exp.max():.2f}, mean={pot_wt_exp.mean():.2f}")

# If the scaling was 50 + 150:
# Y_ref = ((Pot_wt - 150) / 50 / constante_aero) ^ (1/3)
constante_aero = 0.5 * 1.225 * np.pi * (0.15**2) * 0.35 # ~0.015152865
pot_exata_exp_original = (pot_wt_exp - 150.0) / 50.0
# handle negatives or values less than 150. If the scaling is 10 instead:
pot_exata_exp_incorrect = pot_wt_exp / 10.0

y_ref_exp_orig = np.zeros_like(pot_wt_exp)
mask_orig = pot_wt_exp > 150
y_ref_exp_orig[mask_orig] = (pot_exata_exp_original[mask_orig] / constante_aero)**(1/3)

y_ref_exp_inc = (pot_exata_exp_incorrect / constante_aero)**(1/3)

# Let's check correlation of these with csv_vento
# Since csv_vento is 2950, and pot_wt_exp is 2910, they are close.
# Let's check cross-correlation at different shifts:
# Case A: Using original scaling back-calculation
best_wind_corr_orig = -1
best_wind_shift_orig = -1
for shift in range(-200, 200):
    # Slice arrays based on shift
    if shift >= 0:
        c_v_slice = csv_vento[shift:shift+len(y_ref_exp_orig)]
        y_ref_slice = y_ref_exp_orig[:len(c_v_slice)]
    else:
        y_ref_slice = y_ref_exp_orig[-shift:]
        c_v_slice = csv_vento[:len(y_ref_slice)]
    
    if len(c_v_slice) < 500:
        continue
    corr = np.corrcoef(c_v_slice, y_ref_slice)[0, 1]
    if corr > best_wind_corr_orig:
        best_wind_corr_orig = corr
        best_wind_shift_orig = shift

print(f"\nWind Reference Alignment (Original scaling back-calculation):")
print(f"Best correlation: {best_wind_corr_orig:.4f} at shift {best_wind_shift_orig}")

# Case B: Using incorrect scaling (factor of 10) back-calculation
best_wind_corr_inc = -1
best_wind_shift_inc = -1
for shift in range(-200, 200):
    if shift >= 0:
        c_v_slice = csv_vento[shift:shift+len(y_ref_exp_inc)]
        y_ref_slice = y_ref_exp_inc[:len(c_v_slice)]
    else:
        y_ref_slice = y_ref_exp_inc[-shift:]
        c_v_slice = csv_vento[:len(y_ref_slice)]
    
    if len(c_v_slice) < 500:
        continue
    corr = np.corrcoef(c_v_slice, y_ref_slice)[0, 1]
    if corr > best_wind_corr_inc:
        best_wind_corr_inc = corr
        best_wind_shift_inc = shift

print(f"Wind Reference Alignment (Incorrect scaling * 10 back-calculation):")
print(f"Best correlation: {best_wind_corr_inc:.4f} at shift {best_wind_shift_inc}")
