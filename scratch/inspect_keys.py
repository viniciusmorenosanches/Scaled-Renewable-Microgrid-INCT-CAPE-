import scipy.io

mat_path = r'c:\Users\johan\Documents\GitHub\Microrrede INCT CAPE\MGCC_microrrede\dados_cenario_24h_reformulado.mat'
mat = scipy.io.loadmat(mat_path)
for k, v in mat.items():
    if not k.startswith('__'):
        print(f"Key: {k}, type: {type(v)}, shape: {v.shape if hasattr(v, 'shape') else 'no shape'}")
