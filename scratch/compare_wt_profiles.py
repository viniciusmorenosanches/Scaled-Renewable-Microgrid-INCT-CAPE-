import scipy.io
import numpy as np

dir_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede'
mat_cenario = scipy.io.loadmat(dir_path + r'\dados_cenario_24h_reformulado.mat')
mat_final = scipy.io.loadmat(dir_path + r'\dados_microrrede_final.mat')

pot_pv_cenario = mat_cenario['pot_pv'].flatten()
pot_pv_final = mat_final['Pot_pv'].flatten()

pot_pv_final_active = pot_pv_final[51:2451]

# Let's check the correlation between pot_pv_cenario and pot_pv_final_active
corr_pv = np.corrcoef(pot_pv_cenario, pot_pv_final_active)[0, 1]
print(f"Correlation between simulation pot_pv and experimental Pot_pv (active period): {corr_pv:.4f}")

# Let's check the correlation between the solar reference and experimental Pot_pv
def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

csv_sol = load_csv(dir_path + r'\referenciaSol.csv')
corr_sol_ref = np.corrcoef(csv_sol, pot_pv_final_active)[0, 1]
print(f"Correlation between referenciaSol.csv and experimental Pot_pv (active period): {corr_sol_ref:.4f}")
