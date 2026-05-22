import scipy.io
import numpy as np

# Load CSV files
def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
csv_vento = load_csv(dir_path + r'\referenciaVento.csv')

# Load final experimental data
mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')
pot_wt_exp = mat_final['Pot_wt'].flatten()
pot_wt_active = pot_wt_exp[51:2451]

# Let's check the correlation between pot_wt_active and csv_vento at various shifts
best_corr = 0.0
best_shift = -1
for shift in range(-500, 500):
    if shift >= 0:
        c_v_slice = csv_vento[shift:shift+len(pot_wt_active)]
        wt_slice = pot_wt_active[:len(c_v_slice)]
    else:
        wt_slice = pot_wt_active[-shift:]
        c_v_slice = csv_vento[:len(wt_slice)]
    
    if len(c_v_slice) < 1000:
        continue
    corr = np.corrcoef(c_v_slice, wt_slice)[0, 1]
    if abs(corr) > abs(best_corr):
        best_corr = corr
        best_shift = shift

print(f"\nCorrected search:")
print(f"Best correlation between pot_wt_active and csv_vento: {best_corr:.4f} at shift {best_shift}")
