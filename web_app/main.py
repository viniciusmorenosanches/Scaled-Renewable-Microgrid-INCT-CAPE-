import os
import json
import math
import time
import asyncio
from queue import Empty
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, UploadFile, File, Form
from fastapi.responses import HTMLResponse, JSONResponse
from serial_link import MicrogridSerialLink
from controllers.custom import CustomController
from controllers.manual import ManualController
from controllers.matlab_mpc import MatlabMpcController

app = FastAPI()
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Global instances
serial_link = MicrogridSerialLink()
controllers = {
    "custom": CustomController(),
    "manual": ManualController(),
    "matlab_mpc": MatlabMpcController()
}
active_controller_key = "manual"
active_port = "MOCK"

# Scenario profiles database
profiles_db = {
    "solar": {
        "ensolarado": [],
        "nublado": []
    },
    "wind": {
        "normal": [],
        "ventania": []
    },
    "demand": {
        "residencial": []
    }
}

# Current active profile selection keys
active_profile_selection = {
    "solar": "ensolarado",
    "wind": "normal",
    "demand": "residencial"
}

profile_index = 0
profile_active = False

# Persistent telemetry state (prevents UI flicker when queue is temporarily empty)
latest_sensors = {
    "H2_Level": 0.0,
    "Pot_cc": 0.0,
    "Pot_total": 0.0,
    "SoC": 60.0,
    "Pot_wt": 0.0,
    "Pot_pv": 0.0,
    "Pc_actual": 0.0,
    "Pd_actual": 0.0
}
h2_tare_offset = 0.0
h2_max_mm = 47.0

import random
try:
    import scipy.io as sio
except ImportError:
    sio = None

# Path to MGCC_microrrede folder
mgcc_folder = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "MGCC_microrrede")

# 1. Load Wind references from referenciaVento.csv (2400 points, 0.0V - 4.37V)
wind_csv_path = os.path.join(mgcc_folder, "referenciaVento.csv")
profiles_db["wind"]["bancada_24h"] = []
if os.path.exists(wind_csv_path):
    try:
        with open(wind_csv_path, "r", encoding="utf-8") as f:
            for line in f:
                for x in line.strip().split(","):
                    if x.strip():
                        profiles_db["wind"]["bancada_24h"].append(round(float(x.strip()), 3))
    except Exception as e:
        print(f"[Profiles] Error loading wind CSV: {e}")

# 2. Load Solar references from referenciaSol.csv (2400 points, 0.0V - 1.40V)
sol_csv_path = os.path.join(mgcc_folder, "referenciaSol.csv")
profiles_db["solar"]["bancada_24h"] = []
if os.path.exists(sol_csv_path):
    try:
        with open(sol_csv_path, "r", encoding="utf-8") as f:
            for line in f:
                for x in line.strip().split(","):
                    if x.strip():
                        profiles_db["solar"]["bancada_24h"].append(round(float(x.strip()), 3))
    except Exception as e:
        print(f"[Profiles] Error loading solar CSV: {e}")

# 3. Load Demand from referenciaDemanda.csv (primary, zero-dependency) or .mat (if scipy installed)
demand_csv_path = os.path.join(mgcc_folder, "referenciaDemanda.csv")
mat_path = os.path.join(mgcc_folder, "dados_cenario_24h_reformulado.mat")
profiles_db["demand"]["bancada_24h"] = []
if os.path.exists(demand_csv_path):
    try:
        with open(demand_csv_path, "r", encoding="utf-8") as f:
            for line in f:
                for x in line.strip().split(","):
                    if x.strip():
                        profiles_db["demand"]["bancada_24h"].append(round(float(x.strip()), 1))
    except Exception as e:
        print(f"[Profiles] Error loading demand CSV: {e}")
elif sio is not None and os.path.exists(mat_path):
    try:
        mat_data = sio.loadmat(mat_path)
        if "demanda" in mat_data:
            profiles_db["demand"]["bancada_24h"] = [round(float(x), 1) for x in mat_data["demanda"].flatten()]
    except Exception as e:
        print(f"[Profiles] Error loading demand .mat: {e}")

# Generate alternative synthetic profiles for fallback/testing
profiles_db["demand"]["residencial"] = []
for i in range(1200):
    t_hour = (i / 1200.0) * 24.0
    val = 80.0 + 100.0 * math.exp(-((t_hour - 8.0)**2)/2.0) + 150.0 * math.exp(-((t_hour - 19.0)**2)/4.0)
    profiles_db["demand"]["residencial"].append(round(val, 1))

profiles_db["solar"]["ensolarado"] = []
profiles_db["solar"]["nublado"] = []
for i in range(1200):
    t_hour = (i / 1200.0) * 24.0
    if 6 < t_hour < 18:
        val = (200.0 / 150.0) * math.sin((t_hour - 6) * math.pi / 12)
    else:
        val = 0.0
    profiles_db["solar"]["ensolarado"].append(round(val, 3))
    profiles_db["solar"]["nublado"].append(round(val * 0.4, 3))

profiles_db["wind"]["normal"] = []
profiles_db["wind"]["ventania"] = []
for i in range(1200):
    t_hour = (i / 1200.0) * 24.0
    speed = 2.0 + 1.5 * math.sin(t_hour * 1.5)
    speed = max(0.0, min(4.3, speed))
    profiles_db["wind"]["normal"].append(round(speed, 2))
    profiles_db["wind"]["ventania"].append(round(min(4.3, speed * 1.3), 2))

active_profile_selection = {
    "solar": "bancada_24h" if profiles_db["solar"]["bancada_24h"] else "ensolarado",
    "wind": "bancada_24h" if profiles_db["wind"]["bancada_24h"] else "normal",
    "demand": "bancada_24h" if profiles_db["demand"]["bancada_24h"] else "residencial"
}

# New Global Simulation History Buffer
simulation_history = []


@app.on_event("startup")
def startup_event():
    print("[Server] FastAPI started. Profiles database loaded:")
    print(f"  - Solar options: {list(profiles_db['solar'].keys())}")
    print(f"  - Wind options: {list(profiles_db['wind'].keys())}")
    print(f"  - Demand options: {list(profiles_db['demand'].keys())}")

@app.on_event("shutdown")
def startup_shutdown():
    print("[Server] Shutting down. Closing serial link...")
    serial_link.desconectar()

@app.get("/")
def get_index():
    """Serves the main frontend dashboard interface."""
    index_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "templates", "index.html")
    if os.path.exists(index_path):
        with open(index_path, "r", encoding="utf-8") as f:
            return HTMLResponse(f.read())
    return HTMLResponse("<h3>Frontend templates/index.html not found.</h3>", status_code=404)

@app.post("/api/connect")
def api_connect(port: str = Form(...)):
    global active_port
    active_port = port
    serial_link.desconectar()
    success = serial_link.conectar(port)
    return {"success": success, "port": port, "mock": serial_link.mock_mode}

@app.post("/api/disconnect")
def api_disconnect():
    serial_link.desconectar()
    return {"success": True}

@app.post("/api/tare_h2")
def api_tare_h2():
    global h2_tare_offset
    h2_tare_offset = latest_sensors["H2_Level"]
    print(f"[Server] H2 Tared. New offset: {h2_tare_offset}")
    return {"success": True, "offset": h2_tare_offset}

@app.post("/api/set_h2_max")
def api_set_h2_max(max_mm: float = Form(None)):
    global h2_max_mm
    if max_mm is not None and max_mm > 0:
        h2_max_mm = max_mm
    else:
        curr_mm = latest_sensors["H2_Level"] - h2_tare_offset
        if curr_mm > 0:
            h2_max_mm = curr_mm
    print(f"[Server] H2 Max Set. New max_mm: {h2_max_mm}")
    return {"success": True, "max_mm": h2_max_mm}

@app.get("/api/status")
def api_status():
    mpc_ctrl = controllers["matlab_mpc"]
    manual_ctrl = controllers["manual"]
    return {
        "connected": serial_link.is_connected,
        "port": serial_link.port,
        "mock": serial_link.mock_mode,
        "controller": active_controller_key,
        "profile_active": profile_active,
        "profile_index": profile_index,
        "selections": active_profile_selection,
        "Q_soc": mpc_ctrl.Q_soc,
        "Q_h2": mpc_ctrl.Q_h2,
        "Q_load": mpc_ctrl.Q_load,
        "manual_Pc": manual_ctrl.Pc,
        "manual_Pd": manual_ctrl.Pd,
        "manual_ucc": manual_ctrl.ucc,
        "manual_uel": manual_ctrl.uel,
        "manual_ref_sol": manual_ctrl.ref_sol,
        "manual_ref_vento": manual_ctrl.ref_vento,
        "h2_tare_offset": h2_tare_offset,
        "h2_max_mm": h2_max_mm,
        "available_profiles": {
            "solar": list(profiles_db["solar"].keys()),
            "wind": list(profiles_db["wind"].keys()),
            "demand": list(profiles_db["demand"].keys())
        }
    }

@app.post("/api/settings")
def api_settings(
    controller: str = Form(...),
    Pc: float = Form(0.0),
    Pd: float = Form(0.0),
    ucc: int = Form(0),
    uel: int = Form(0),
    ref_sol: float = Form(0.0),
    ref_vento: float = Form(0.0),
    Q_soc: float = Form(1.0),
    Q_h2: float = Form(1.0),
    Q_load: float = Form(100.0)
):
    global active_controller_key
    if controller in controllers:
        active_controller_key = controller
        if controller == "manual":
            controllers["manual"].update_manual_values(Pc, Pd, ucc, uel, ref_sol, ref_vento)
        elif controller == "matlab_mpc":
            controllers["matlab_mpc"].update_weights(Q_soc, Q_h2, Q_load)
        return {"success": True, "controller": controller}
    return {"success": False, "error": "Invalid controller"}

@app.post("/api/profile/select")
def api_profile_select(profile_type: str = Form(...), value: str = Form(...)):
    """Handles dropdown profile selections."""
    if profile_type in active_profile_selection:
        if value in profiles_db[profile_type]:
            active_profile_selection[profile_type] = value
            return {"success": True, "type": profile_type, "selected": value}
        return {"success": False, "error": f"Profile '{value}' not found in database"}
    return {"success": False, "error": "Invalid profile type"}

@app.post("/api/profile/toggle")
def api_profile_toggle(active: bool = Form(...)):
    global profile_active, profile_index, simulation_history
    profile_active = active
    if not active:
        profile_index = 0
    else:
        # Reset simulation history on start
        profile_index = 0
        simulation_history.clear()
        
    return {"profile_active": profile_active, "index": profile_index}

@app.post("/api/upload")
async def api_upload(profile_type: str = Form(...), file: UploadFile = File(...)):
    try:
        content = await file.read()
        text = content.decode("utf-8").replace("\n", ",").replace("\r", ",")
        values = [float(x.strip()) for x in text.split(",") if x.strip()]
        
        if profile_type in profiles_db:
            filename = os.path.splitext(file.filename)[0]
            custom_name = f"Custom: {filename}"
            profiles_db[profile_type][custom_name] = values
            
            # Automatically set as active profile
            active_profile_selection[profile_type] = custom_name
            
            print(f"[Server] Uploaded custom {profile_type} profile '{custom_name}': {len(values)} samples")
            return {"success": True, "name": custom_name, "length": len(values)}
            
        return {"success": False, "error": "Invalid profile type"}
    except Exception as e:
        return {"success": False, "error": str(e)}

@app.post("/api/controller/upload")
async def api_controller_upload(file: UploadFile = File(...)):
    try:
        filename = file.filename
        ext = os.path.splitext(filename)[1].lower()
        if ext not in [".py", ".m"]:
            return {"success": False, "error": "Apenas arquivos .py ou .m são permitidos"}
        
        content = await file.read()
        if ext == ".py":
            dest_path = os.path.join(BASE_DIR, "web_app", "controllers", "custom_uploaded.py")
            with open(dest_path, "wb") as f:
                f.write(content)
            return {"success": True, "type": "py", "filename": filename}
        elif ext == ".m":
            dest_path = os.path.join(BASE_DIR, "MGCC_microrrede", "custom_uploaded.m")
            with open(dest_path, "wb") as f:
                f.write(content)
            return {"success": True, "type": "m", "filename": filename}
    except Exception as e:
        return {"success": False, "error": str(e)}

from fastapi.responses import Response

@app.get("/api/download_csv")
def download_csv():
    global simulation_history
    if not simulation_history:
        return Response(content="No data available. Run a simulation first.", status_code=404)
    
    # Generate CSV content
    headers = list(simulation_history[0].keys())
    csv_lines = [",".join(headers)]
    
    for row in simulation_history:
        csv_lines.append(",".join(str(row[h]) for h in headers))
        
    csv_content = "\n".join(csv_lines)
    
    timestamp = time.strftime("%Y%m%d_%H%M%S")
    return Response(
        content=csv_content,
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename=relatorio_microrrede_{timestamp}.csv"}
    )

@app.get("/api/simulation_history")
def get_simulation_history():
    return {"history": simulation_history}

@app.get("/api/profile/data")
def get_profile_data():
    active_sol_key = active_profile_selection["solar"]
    active_win_key = active_profile_selection["wind"]
    active_dem_key = active_profile_selection["demand"]
    
    sol_profile = profiles_db["solar"].get(active_sol_key, [])
    win_profile = profiles_db["wind"].get(active_win_key, [])
    dem_profile = profiles_db["demand"].get(active_dem_key, [])

    sol_out, win_out, dem_out = [], [], []
    for i in range(1200):
        ref_sol = sol_profile[i % len(sol_profile)] if sol_profile else 0.0
        ref_win = win_profile[i % len(win_profile)] if win_profile else 0.0
        
        pot_pv = 150.0 * ref_sol
        pot_pv = max(0.0, pot_pv)
        
        pot_wt = max(0.0, round(10.0 * ref_win, 1))
        
        sol_out.append(pot_pv)
        win_out.append(pot_wt)
        dem_out.append(dem_profile[i % len(dem_profile)] if dem_profile else 0.0)

    return {"solar": sol_out, "wind": win_out, "demand": dem_out}

@app.get("/api/mock/data")
def get_mock_data():
    active_sol_key = active_profile_selection["solar"]
    active_win_key = active_profile_selection["wind"]
    active_dem_key = active_profile_selection["demand"]
    
    sol_profile = profiles_db["solar"].get(active_sol_key, [])
    win_profile = profiles_db["wind"].get(active_win_key, [])
    dem_profile = profiles_db["demand"].get(active_dem_key, [])

    sol_out, win_out, dem_out = [], [], []
    bat_out, h2_out, total_out = [], [], []
    soc_out, h2_lvl_out = [], []

    current_soc = 50.0
    current_h2_level = 50.0

    for i in range(1200):
        ref_sol = sol_profile[i % len(sol_profile)] if sol_profile else 0.0
        ref_win = win_profile[i % len(win_profile)] if win_profile else 0.0
        demand = dem_profile[i % len(dem_profile)] if dem_profile else 0.0
        
        pot_pv = 150.0 * ref_sol
        pot_pv = max(0.0, pot_pv)
        
        pot_wt = max(0.0, round(10.0 * ref_win, 1))
        
        sol_out.append(pot_pv)
        win_out.append(pot_wt)
        dem_out.append(demand)

        generation = pot_pv + pot_wt
        net_load = demand - generation

        pot_bat = 0.0
        pot_h2 = 0.0

        if net_load > 0:
            pot_bat = min(net_load, 30.0)
            net_load -= pot_bat
            if net_load > 0 and current_h2_level > 5.0:
                pot_h2 = 20.0
        elif net_load < 0:
            excess = -net_load
            pot_bat = -min(excess, 50.0)
            excess -= (-pot_bat)
            if excess > 0 and current_h2_level < 95.0:
                pot_h2 = -20.0
                
        current_soc -= (pot_bat * 1.2) / 36.0 
        if pot_h2 > 0:
            current_h2_level -= 0.05
        elif pot_h2 < 0:
            current_h2_level += 0.05
            
        current_soc = max(10.0, min(100.0, current_soc))
        current_h2_level = max(5.0, min(95.0, current_h2_level))

        total_supply = generation + pot_bat + pot_h2

        bat_out.append(pot_bat)
        h2_out.append(pot_h2)
        total_out.append(total_supply)
        soc_out.append(current_soc)
        h2_lvl_out.append(current_h2_level)

    return {
        "solar": sol_out, "wind": win_out, "demand": dem_out,
        "battery": bat_out, "h2": h2_out, "total": total_out,
        "soc": soc_out, "h2_level": h2_lvl_out
    }
@app.websocket("/ws/telemetry")
async def websocket_endpoint(websocket: WebSocket):
    global profile_index, profile_active, latest_sensors, simulation_history
    await websocket.accept()
    print("[WebSocket] Client connected.")
    try:
        while True:
            # Dynamic loop interval: 10 Hz (0.1s) when simulation is running, 5 Hz (0.2s) when idle/manual
            dt = 0.1 if profile_active else 0.2
            serial_link.sleep_time = dt

            # 1. Fetch sensor readings from physical/mock serial link queue
            if serial_link.is_connected:
                try:
                    new_sensors = serial_link.sensor_data_queue.get_nowait()
                    latest_sensors.update(new_sensors)
                except Empty:
                    pass
            else:
                latest_sensors["Pot_pv"] = 0.0
                latest_sensors["Pot_wt"] = 0.0
                latest_sensors["Pot_cc"] = 0.0
                latest_sensors["Pot_total"] = 0.0
            
            sensors = latest_sensors.copy()
            # Apply H2 Tare Offset and calculate 0-100%
            raw_h2 = sensors["H2_Level"]
            h2_mm = raw_h2 - h2_tare_offset
            sensors["H2_Level"] = h2_mm
            
            h2_pct = (h2_mm / h2_max_mm * 100.0) if h2_max_mm > 0 else 0.0
            h2_pct = max(0.0, min(100.0, h2_pct))
            
            # Normalize battery SoC (0-100%)
            raw_soc = float(sensors.get("SoC", 0.0))
            soc_pct = (raw_soc * 100.0) if raw_soc <= 1.05 else raw_soc
            soc_pct = max(0.0, min(100.0, soc_pct))
            sensors["SoC"] = soc_pct

            # 2. Extract active profiles references
            active_sol_key = active_profile_selection["solar"]
            active_win_key = active_profile_selection["wind"]
            active_dem_key = active_profile_selection["demand"]
            
            sol_profile = profiles_db["solar"].get(active_sol_key, [])
            win_profile = profiles_db["wind"].get(active_win_key, [])
            dem_profile = profiles_db["demand"].get(active_dem_key, [])

            ref_sol = 0.0
            ref_vento = 0.0
            demand_val = 0.0
            
            # Auto-stop when profile finishes
            if profile_active and profile_index >= 1200:
                profile_active = False
                profile_index = 0
                await websocket.send_text(json.dumps({"type": "simulation_complete"}))
            
            if profile_active:
                sol_len = len(sol_profile)
                win_len = len(win_profile)
                dem_len = len(dem_profile)
                
                if sol_len > 0:
                    ref_sol = sol_profile[profile_index % sol_len]
                if win_len > 0:
                    ref_vento = win_profile[profile_index % win_len]
                if dem_len > 0:
                    demand_val = dem_profile[profile_index % dem_len]
                else:
                    t_hour = (profile_index * dt) / 60.0
                    t_hour = t_hour % 24.0
                    demand_val = 80.0 + 100.0 * math.exp(-((t_hour - 8.0)**2)/2.0) + 150.0 * math.exp(-((t_hour - 19.0)**2)/4.0)

                profile_index += 1
            else:
                if active_controller_key == "manual":
                    ref_sol = controllers["manual"].ref_sol
                    ref_vento = controllers["manual"].ref_vento
                else:
                    ref_sol = 0.0
                    ref_vento = 0.0
                demand_val = 0.0

            # Feed references into sensor state for controller calculations
            sensors["ref_sol"] = ref_sol
            sensors["ref_vento"] = ref_vento
            sensors["Demand"] = demand_val

            # 3. Process controller algorithm (Modular Brain)
            if active_controller_key != "manual" and not profile_active:
                # Force safe idle state for automatic controllers when profile is not running
                controls = {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "info": "Automatic controller idle"}
            else:
                controller = controllers[active_controller_key]
                controls = controller.compute_controls(sensors)
            
            controls["ref_sol"] = ref_sol
            controls["ref_vento"] = ref_vento

            # 4. Write controls back to serial link queue
            if serial_link.is_connected:
                if serial_link.control_data_queue.full():
                    try: serial_link.control_data_queue.get_nowait()
                    except Empty: pass
                serial_link.control_data_queue.put(controls)

            # 5. Broadcast packaged JSON telemetry over WebSockets to client UI
            h2_contrib = sensors["Pot_cc"] - (10.0 if controls["uel"] == 1 else 0.0)
            total_supplied = sensors["Pot_pv"] + sensors["Pot_wt"] + h2_contrib - controls["Pc"] + controls["Pd"]
            deviation = total_supplied - demand_val

            payload = {
                "type": "telemetry",
                "connected": serial_link.is_connected,
                "Time": time.strftime("%H:%M:%S"),
                "Solar": round(sensors["Pot_pv"], 1),
                "Wind": round(sensors["Pot_wt"], 1),
                "Demand": round(demand_val, 1),
                "Battery": round(controls["Pd"] - controls["Pc"], 1),
                "H2": round(h2_contrib, 1),
                "Total_Supply": round(total_supplied, 1),
                "Deviation": round(deviation, 1),
                "SoC": round(soc_pct, 1),
                "H2_Level": round(h2_mm, 2),
                "H2_Pct": round(h2_pct, 1),
                "H2_Max_mm": round(h2_max_mm, 1),
                "Pc_actual": round(sensors["Pc_actual"], 1),
                "Pd_actual": round(sensors["Pd_actual"], 1),
                "controller": active_controller_key,
                "profile_active": profile_active,
                "profile_index": profile_index
            }
            
            # Log data into simulation history if profile is running
            if profile_active:
                simulation_history.append({
                    "Step": profile_index,
                    "Ref_Sol": ref_sol,
                    "Ref_Vento": ref_vento,
                    "Demanda": demand_val,
                    "Pot_Solar_Real": sensors["Pot_pv"],
                    "Pot_Vento_Real": sensors["Pot_wt"],
                    "SoC": soc_pct,
                    "Tanque_H2_mm": round(h2_mm, 2),
                    "Tanque_H2_Pct": round(h2_pct, 1),
                    "Pc_Aplicado": controls["Pc"],
                    "Pd_Aplicado": controls["Pd"],
                    "Ucc": controls["ucc"],
                    "Uel": controls["uel"],
                    "Desvio": deviation
                })
            
            await websocket.send_text(json.dumps(payload))
            await asyncio.sleep(dt)

    except WebSocketDisconnect:
        print("[WebSocket] Client disconnected.")
    except Exception as e:
        print(f"[WebSocket] Loop exception: {e}")
