import streamlit as st
import pandas as pd
import numpy as np
import plotly.graph_objects as go
from datetime import datetime, timedelta
import time
from serial_link import MicrogridSerialLink 

# Inicialização da memória de ligação
if "link_serial" not in st.session_state: st.session_state.link_serial = None
if "hardware_conectado" not in st.session_state: st.session_state.hardware_conectado = False
if "files_solar" not in st.session_state: st.session_state.files_solar = []
if "files_wind" not in st.session_state: st.session_state.files_wind = []
if "files_demand" not in st.session_state: st.session_state.files_demand = []


# NOVO: Cria um histórico vazio para desenhar os gráficos reais
if "df_historico" not in st.session_state:
    st.session_state.df_historico = pd.DataFrame({
        "Time": [], "Solar": [], "Wind": [], "Demand": [],
        "Battery": [], "H2": [], "Total_Supply": [], "Deviation": [], "SoC": [], "H2_Level": []
    })

# ==========================================
# 1. CONFIGURAÇÃO DA PÁGINA
# ==========================================
st.set_page_config(page_title="Microgrid Supervisor", layout="wide", page_icon="⚡")

st.markdown("""
    <style>
    .block-container { padding-top: 1.5rem; padding-bottom: 1rem; }
    div[data-testid="metric-container"] { background-color: #1E1E1E; border: 1px solid #333; padding: 15px; border-radius: 8px; }
    div[data-testid="stMetricValue"] { font-size: 1.8rem; color: #4CAF50;}
    [data-testid="stPopover"] { margin-top: 8px; } 
    </style>
    """, unsafe_allow_html=True)

# ==========================================
# 2. DADOS FALSOS (Para Visualização sem Placa)
# ==========================================

@st.cache_data
def generate_dummy_data():

    times = [datetime(2026, 3, 4, 0, 0) + timedelta(minutes=10*i) for i in range(144)]

    x = np.linspace(0, 24, 144)

    solar = np.where(
        (x > 6) & (x < 18),
        200 * np.sin((x - 6) * np.pi / 12),
        0
    )

    wind = 50 + 20 * np.sin(x) + np.random.normal(0, 5, 144)

    demand = (
        80
        + 100 * np.exp(-((x - 8)**2)/2)
        + 150 * np.exp(-((x - 19)**2)/4)
    )

    net_load = demand - (solar + wind)

    bat_power = np.zeros(144)
    h2_power = np.zeros(144)

    POTENCIA_FC = 50
    POTENCIA_ELY = -80

    for i in range(144):

        if net_load[i] > 0:

            if net_load[i] > 100:
                h2_power[i] = POTENCIA_FC
                bat_power[i] = net_load[i] - POTENCIA_FC

            else:
                h2_power[i] = 0
                bat_power[i] = net_load[i]

        else:

            if net_load[i] < -100:
                h2_power[i] = POTENCIA_ELY
                bat_power[i] = net_load[i] - POTENCIA_ELY

            else:
                h2_power[i] = 0
                bat_power[i] = net_load[i]

    total_supplied = solar + wind + bat_power + h2_power

    deviation = total_supplied - demand

    soc = 50 - np.cumsum(bat_power) * 0.05
    soc = np.clip(soc, 0, 100)

    h2_level = 20 - np.cumsum(h2_power) * 0.02
    h2_level = np.clip(h2_level, 0, 100)

    return pd.DataFrame({
        "Time": times,
        "Solar": solar,
        "Wind": wind,
        "Demand": demand,
        "Battery": bat_power,
        "H2": h2_power,
        "Total_Supply": total_supplied,
        "Deviation": deviation,
        "SoC": soc,
        "H2_Level": h2_level
    })


df = generate_dummy_data()

# ==========================================
# 3. BARRA LATERAL
# ==========================================
with st.sidebar:
    st.header("⚡ Painel de Controle")
    st.divider()

    with st.expander("🔌 Conexão Física (Cabo USB)", expanded=True):
        porta_com = st.text_input("Porta Serial (ex: COM3)", value="COM3")
        c1, c2 = st.columns(2)
        if c1.button("Conectar"):
            st.session_state.link_serial = MicrogridSerialLink(porta_com)
            if st.session_state.link_serial.conectar():
                st.session_state.hardware_conectado = True
                st.success("Conectado!")
            else:
                st.error("Erro na porta.")
        if c2.button("Desconectar"):
            if st.session_state.link_serial: st.session_state.link_serial.desconectar()
            st.session_state.hardware_conectado = False
            st.session_state.link_serial = None
            st.warning("Desconectado.")
    
    with st.expander("📂 Perfis de Entrada", expanded=True):
        solar_opts = ["Perfil Ensolarado", "Perfil Nublado"] + st.session_state.files_solar + ["Adicionar novo perfil..."]
        sel_solar = st.selectbox("Perfil Solar", solar_opts)
        if sel_solar == "Adicionar novo perfil...":
            f_sol = st.file_uploader("Upload CSV Solar", type=["csv", "mat"], key="up_sol")
            if f_sol and f_sol.name not in st.session_state.files_solar:
                st.session_state.files_solar.append(f_sol.name)
                st.rerun()

        wind_opts = ["Perfil Normal", "Perfil Ventania"] + st.session_state.files_wind + ["Adicionar novo perfil..."]
        sel_wind = st.selectbox("Perfil Eólico", wind_opts)
        if sel_wind == "Adicionar novo perfil...":
            f_win = st.file_uploader("Upload CSV Eólica", type=["csv", "mat"], key="up_win")
            if f_win and f_win.name not in st.session_state.files_wind:
                st.session_state.files_wind.append(f_win.name)
                st.rerun()

        demand_opts = ["Perfil Residencial (Base)"] + st.session_state.files_demand + ["Adicionar novo perfil..."]
        sel_demand = st.selectbox("Perfil de Demanda", demand_opts)
        if sel_demand == "Adicionar novo perfil...":
            f_dem = st.file_uploader("Upload CSV Demanda", type=["csv", "mat"], key="up_dem")
            if f_dem and f_dem.name not in st.session_state.files_demand:
                st.session_state.files_demand.append(f_dem.name)
                st.rerun()
                
        if len(st.session_state.files_solar) > 0 or len(st.session_state.files_wind) > 0 or len(st.session_state.files_demand) > 0:
            if st.button("🗑️ Limpar Arquivos"):
                st.session_state.files_solar.clear()
                st.session_state.files_wind.clear()
                st.session_state.files_demand.clear()
                st.rerun()

    with st.expander("🧠 Estratégia de Controle", expanded=True):
        mode = st.radio("Modo de Operação", ["Automático (MPC Base)", "Algoritmo Externo (Customizado)", "Acionamento Manual"])
        
        if mode == "Automático (MPC Base)":
            st.markdown("**Pesos do Controlador (MPC)**")
            st.slider("Seguir Refer. Bateria ($Q_{soc}$)", 0.0, 1.0, 0.5)
            st.slider("Seguir Refer. H2 ($Q_{h2}$)", 0.0, 1.0, 0.2)
            st.slider("Priorizar Demanda ($Q_{load}$)", 0.0, 1.0, 0.8)
            
            # O Botão só aparece no modo Automático agora!
            if st.button("▶ Executar Ciclo de Controle", type="primary", use_container_width=True):
                # Aqui futuramente vai a lógica do seu código MPC
                st.info("Ciclo MPC executado.")
                
        elif mode == "Algoritmo Externo (Customizado)":
            st.info("Faça upload do seu script Python ou Matlab.")
            st.file_uploader("Upload do Controlador", type=["py", "m"])
                
        elif mode == "Acionamento Manual":
            slider_bateria = st.slider("Carga/Descarga Bateria (W)", -200, 200, 0)
            st.markdown("Sistema de Hidrogênio (On/Off)")
            c1, c2 = st.columns(2)
            toggle_fc = c1.toggle("Fuel Cell")
            toggle_ely = c2.toggle("Eletrolisador")

            # --- ENVIO E REGISTO INSTANTÂNEO ---
            if st.session_state.hardware_conectado and st.session_state.link_serial:
                cmd_pc = abs(slider_bateria) if slider_bateria < 0 else 0
                cmd_pd = slider_bateria if slider_bateria > 0 else 0
                cmd_ucc = 1 if toggle_fc else 0
                cmd_uel = 1 if toggle_ely else 0
                
                # 1. Envia a ordem
                st.session_state.link_serial.enviar_comandos(cmd_pc, cmd_pd, cmd_ucc, cmd_uel, 150.0, 80.0)
                
                # 2. Lê os sensores imediatamente a seguir
                dados_reais = st.session_state.link_serial.ler_sensores()
                
                if dados_reais:
                    # 3. Adiciona a nova leitura à tabela do histórico
                    novo_dado = {
                        "Time": datetime.now(),
                        "Solar": dados_reais["Pot_pv"],
                        "Wind": dados_reais["Pot_wt"],
                        "Demand": dados_reais["Pc_medido"], # Ajustar conforme a lógica
                        "Battery": dados_reais["Pd_medido"] - dados_reais["Pc_medido"],
                        "H2": dados_reais["Pot_cc"],
                        "Total_Supply": dados_reais["Pot_total"],
                        "Deviation": 0,
                        "SoC": dados_reais["SoC"],
                        "H2_Level": dados_reais["H2_Level"]
                    }
                    
                    # Concatena o novo dado e mantém apenas os últimos 60 pontos para não pesar a RAM
                    st.session_state.df_historico = pd.concat([st.session_state.df_historico, pd.DataFrame([novo_dado])], ignore_index=True)
                    if len(st.session_state.df_historico) > 60:
                        st.session_state.df_historico = st.session_state.df_historico.iloc[-60:]

                st.caption(f"⚡ Status Atualizado: FC={cmd_ucc} | ELY={cmd_uel}")

   
# ==========================================
# 4. TELA PRINCIPAL
# ==========================================
st.title("Gêmeo Digital da Microrrede")

# Puxa os dados reais da memória!
if st.session_state.df_historico.empty:
    df_plot = df
else:
    df_plot = st.session_state.df_historico


# Se a tabela estiver vazia (ainda não clicou no botão), mostra um aviso em vez de dar erro
if df_plot.empty:
    st.info("🔌 Aguardando conexão... Vá ao painel lateral, conecte a porta COM e envie um comando manual para iniciar a telemetria.")
else:
    k1, k2, k3, k4, k5 = st.columns(5)
    
    # iloc[-1] pega a última linha da tabela (o dado mais recente!)
    k1.metric("Relógio", df_plot["Time"].iloc[-1].strftime("%H:%M:%S"))
    k2.metric("Demanda", f"{df_plot['Demand'].iloc[-1]:.0f} W")
    k3.metric("Geração Renovável", f"{(df_plot['Solar'].iloc[-1] + df_plot['Wind'].iloc[-1]):.0f} W")
    k4.metric("SoC Bateria", f"{df_plot['SoC'].iloc[-1]:.0f} %")
    k5.metric("Tanque H2", f"{df_plot['H2_Level'].iloc[-1]:.0f} %")

    st.markdown("<br>", unsafe_allow_html=True)

    col_chart1, col_chart2 = st.columns([5, 3])

    with col_chart1:
        c_title, c_filt = st.columns([3.5, 1.5]) 
        with c_title: st.markdown("### 🔌 Balanço de Potência")
        with c_filt:
            with st.popover("Filtrar Curvas", use_container_width=True):
                show_wind = st.checkbox("Eólica", value=True)
                show_solar = st.checkbox("Solar", value=True)
                show_bat = st.checkbox("Bateria (±)", value=True)
                show_h2  = st.checkbox("Hidrogênio (±)", value=True)
                show_dem = st.checkbox("Linha da Demanda", value=True)
                show_tot = st.checkbox("Soma de Geração", value=True)
                show_err = st.checkbox("Desvio (Erro)", value=False)
        
        fig_power = go.Figure()
        
        # Adicionado mode='lines+markers' para ver cada ponto de leitura claramente
        if show_wind: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Wind"], name="Eólica", mode='lines+markers', fill='tozeroy', line=dict(color='rgba(86, 180, 233, 0.5)', width=2)))
        if show_solar: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Solar"], name="Solar", mode='lines+markers', fill='tozeroy', line=dict(color='rgba(230, 159, 0, 0.5)', width=2)))
        if show_bat: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Battery"], name="Bateria (+Desc/-Carga)", mode='lines+markers', fill='tozeroy', line=dict(color='rgba(204, 121, 167, 0.8)', shape='hv', width=2)))
        if show_h2: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["H2"], name="H2 (+FC/-Eletr)", mode='lines+markers', fill='tozeroy', line=dict(color='rgba(213, 94, 0, 0.8)', shape='hv', width=3))) 
        if show_tot: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Total_Supply"], name="Geração Total", mode='lines+markers', line=dict(color='#009E73', width=2, dash='dash')))
        if show_dem: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Demand"], name="Demanda", mode='lines+markers', line=dict(color='#FFFFFF', width=3)))
        if show_err: fig_power.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["Deviation"], name="Desvio", mode='lines+markers', line=dict(color='#888888', width=2, dash='dashdot')))
        
        fig_power.update_layout(height=480, margin=dict(l=0, r=0, t=10, b=0), legend=dict(orientation="h", yanchor="top", y=-0.15, xanchor="center", x=0.5), plot_bgcolor='rgba(0,0,0,0)', paper_bgcolor='rgba(0,0,0,0)', yaxis=dict(title="Potência (W)", showgrid=True, gridcolor='#333'))
        fig_power.add_hline(y=0, line_width=1, line_color="#888") 
        st.plotly_chart(fig_power, use_container_width=True)

    with col_chart2:
        st.markdown("### 🔋 Níveis de Armazenamento")
        fig_storage = go.Figure()
        fig_storage.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["SoC"], name="Bateria", mode='lines+markers', line=dict(color='#CC79A7', width=3)))
        fig_storage.add_trace(go.Scatter(x=df_plot["Time"], y=df_plot["H2_Level"], name="H2", mode='lines+markers', line=dict(color='#D55E00', width=3)))
        fig_storage.update_layout(height=480, margin=dict(l=0, r=0, t=10, b=0), legend=dict(orientation="h", yanchor="top", y=-0.15, xanchor="center", x=0.5), plot_bgcolor='rgba(0,0,0,0)', paper_bgcolor='rgba(0,0,0,0)', yaxis=dict(title="Carga (%) / Nível (mm)", range=[-5, 105], showgrid=True, gridcolor='#333'))
        st.plotly_chart(fig_storage, use_container_width=True)



        # ==========================================
# 5. MODO LIVE (ATUALIZAÇÃO AUTOMÁTICA)
# ==========================================
with st.sidebar:
    st.divider()
    st.markdown("### 📡 Telemetria")
    auto_update = st.toggle("🔄 Modo Live (Atualização Contínua)", value=False)

if auto_update and st.session_state.hardware_conectado and st.session_state.link_serial:
    
    # 1. Tenta pescar um dado novo do Arduino
    dados_reais = st.session_state.link_serial.ler_sensores()
    
    if dados_reais:
        # 2. Guarda na memória do histórico
        novo_dado = {
            "Time": datetime.now(),
            "Solar": dados_reais["Pot_pv"],
            "Wind": dados_reais["Pot_wt"],
            "Demand": dados_reais["Pc_medido"], 
            "Battery": dados_reais["Pd_medido"] - dados_reais["Pc_medido"],
            "H2": dados_reais["Pot_cc"],
            "Total_Supply": dados_reais["Pot_total"],
            "Deviation": 0,
            "SoC": dados_reais["SoC"],
            "H2_Level": dados_reais["H2_Level"]
        }
        
        st.session_state.df_historico = pd.concat([st.session_state.df_historico, pd.DataFrame([novo_dado])], ignore_index=True)
        
        # Mantém os últimos 100 pontos para o gráfico ficar bonito a rolar
        if len(st.session_state.df_historico) > 100:
            st.session_state.df_historico = st.session_state.df_historico.iloc[-100:]

    # 3. Espera 1 segundo (para não sobrecarregar o CPU do PC)
    time.sleep(1.0)
    
    # 4. Força o Streamlit a rodar o código todo de novo!
    st.rerun()