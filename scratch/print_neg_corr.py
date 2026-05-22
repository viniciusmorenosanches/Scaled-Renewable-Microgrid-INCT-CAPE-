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

# Let's print first 150 values of both at shift -1:
# which means wt_slice = pot_wt_active[1:]
# c_v_slice = csv_vento[:len(wt_slice)]
wt_slice = pot_wt_active[1:]
c_v_slice = csv_vento[:len(wt_slice)]

print("First 50 values of csv_vento:")
print(np.round(c_v_slice[:50], 3))
print("First 50 values of pot_wt_active[1:]:")
print(np.round(wt_slice[:50], 2))

# Wait, why was correlation -1.0000? Let's check:
print("Correlation:", np.corrcoef(c_v_slice, wt_slice)[0, 1])
# Ah, wait! The correlation was -1.0000 at shift -1. Is it because one is flat/zero or has some constant values?
# Let's print the correlation for the entire range or check variance:
print("Var of c_v_slice:", np.var(c_v_slice))
print("Var of wt_slice:", np.var(wt_slice))
# Let's check if the first 20 values of wt_slice are 0.0, and what are the corresponding values of c_v_slice.
# Wait, let's print values from index 20 to 80 of both:
print("\nFrom index 20 to 80:")
print("csv_vento:", np.round(c_v_slice[20:80], 3))
print("pot_wt_active:", np.round(wt_slice[20:80], 2))
