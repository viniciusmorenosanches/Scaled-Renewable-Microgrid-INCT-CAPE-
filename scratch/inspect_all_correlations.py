import scipy.io
import numpy as np

# Load CSV files
def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
csv_sol = load_csv(dir_path + r'\referenciaSol.csv')
csv_vento = load_csv(dir_path + r'\referenciaVento.csv')

mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')

print("Correlations with referenciaSol (length 2400, shift=51):")
for k, v in mat_final.items():
    if not k.startswith('__'):
        flat_v = v.flatten()
        if len(flat_v) >= 2451:
            slice_v = flat_v[51:2451]
            corr = np.corrcoef(csv_sol, slice_v)[0, 1]
            print(f"  {k}: {corr:.4f}")

print("\nCorrelations with referenciaVento (length 2950, sliced to 2400, shift=51):")
csv_vento_slice = csv_vento[51:2451]
for k, v in mat_final.items():
    if not k.startswith('__'):
        flat_v = v.flatten()
        if len(flat_v) >= 2451:
            slice_v = flat_v[51:2451]
            corr = np.corrcoef(csv_vento_slice, slice_v)[0, 1]
            print(f"  {k}: {corr:.4f}")
            
print("\nCorrelations with the back-calculated Y_ref from scenario pot_wt (length 2400, shift=51):")
# Let's load the pot_wt from cenario, back-calculate Y_ref, and correlate it with the final variables:
mat_cenario = scipy.io.loadmat(dir_path + r'\dados_cenario_24h_reformulado.mat')
pot_wt_cen = mat_cenario['pot_wt'].flatten()
# Let's correlate pot_wt_cen with final variables
for k, v in mat_final.items():
    if not k.startswith('__'):
        flat_v = v.flatten()
        if len(flat_v) >= 2451:
            slice_v = flat_v[51:2451]
            corr = np.corrcoef(pot_wt_cen, slice_v)[0, 1]
            print(f"  {k}: {corr:.4f}")
