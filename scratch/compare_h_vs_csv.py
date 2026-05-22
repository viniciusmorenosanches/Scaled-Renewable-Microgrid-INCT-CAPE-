import re
import numpy as np

h_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\ArduinoCentral_Microrrede_INCT_serial\referencias.h'
csv_wind_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede\referenciaVento.csv'
csv_solar_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede\referenciaSol.csv'

# Parse referencias.h
with open(h_path, 'r') as f:
    h_content = f.read()

# Extract arrays
sol_match = re.search(r'referenciaSol\[\d+\][^=]*=\s*\{([^}]+)\}', h_content)
vento_match = re.search(r'referenciaVento\[\d+\][^=]*=\s*\{([^}]+)\}', h_content)

h_sol = np.array([float(x.strip()) for x in sol_match.group(1).split(',') if x.strip()]) if sol_match else None
h_vento = np.array([float(x.strip()) for x in vento_match.group(1).split(',') if x.strip()]) if vento_match else None

def load_csv(path):
    with open(path, 'r') as f:
        content = f.read().replace('\n', ',').replace('\r', ',')
        values = [float(x.strip()) for x in content.split(',') if x.strip()]
    return np.array(values)

csv_sol = load_csv(csv_solar_path)
csv_vento = load_csv(csv_wind_path)

print(f"h_sol length: {len(h_sol) if h_sol is not None else 'None'}")
print(f"csv_sol length: {len(csv_sol)}")
if h_sol is not None:
    print(f"Solar array diff sum: {np.sum(np.abs(h_sol - csv_sol[:len(h_sol)]))}")

print(f"h_vento length: {len(h_vento) if h_vento is not None else 'None'}")
print(f"csv_vento length: {len(csv_vento)}")
if h_vento is not None:
    # They have the same length 2950
    diff = np.sum(np.abs(h_vento - csv_vento[:len(h_vento)]))
    print(f"Wind array diff sum: {diff}")
    if diff > 0:
        print("First 10 values of h_vento:")
        print(h_vento[:10])
        print("First 10 values of csv_vento:")
        print(csv_vento[:10])
