import serial
import threading
import time
import math
from queue import Queue, Empty

class MicrogridSerialLink:
    def __init__(self, port: str = None, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        self.running = False
        self.thread = None
        
        # Connection status
        self.is_connected = False
        self.mock_mode = False
        self.sleep_time = 0.2  # Default to 5 Hz (0.2s sleep)
        
        # Thread-safe queues
        self.sensor_data_queue = Queue(maxsize=1)
        self.control_data_queue = Queue(maxsize=1)
        self.last_controls = {"Pc": 0.0, "Pd": 0.0, "ucc": 0, "uel": 0, "ref_sol": 0.0, "ref_vento": 0.0}
        
        # Mock variables (for simulation without physical board)
        self.mock_soc = 60.0       # battery %
        self.mock_h2_level = 2.7   # water level mm
        self.mock_time = 0.0

    def conectar(self, port: str = None) -> bool:
        if port:
            self.port = port
            
        if self.port == "MOCK" or self.port == "MOCK_MODE":
            self.mock_mode = True
            self.is_connected = True
            self.running = True
            # No thread for mock, removing mock data generation
            print("[Serial] Mock mode initialized without background simulation.")
            return True
            
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=1.0)
            self.mock_mode = False
            self.is_connected = True
            self.running = True
            self.thread = threading.Thread(target=self._hardware_loop, daemon=True)
            self.thread.start()
            print(f"[Serial] Connected to physical board on {self.port} at {self.baudrate} baud.")
            return True
        except Exception as e:
            print(f"[Serial] Failed to open port {self.port}: {e}")
            self.is_connected = False
            return False

    def desconectar(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1.0)
            self.thread = None
            
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            
        self.serial_conn = None
        self.is_connected = False
        self.mock_mode = False
        print("[Serial] Disconnected.")

    def _hardware_loop(self):
        """Persistent thread loop communicating with the physical Arduino Central."""
        while self.running:
            try:
                # 1. Get latest controls from main loop (or last sent if empty)
                try:
                    controls = self.control_data_queue.get_nowait()
                    self.last_controls = controls
                except Empty:
                    controls = self.last_controls

                # 2. Format command string for Arduino
                cmd_string = f"{controls['Pc']:.1f},{controls['Pd']:.1f},{controls['ucc']},{controls['uel']},{controls['ref_sol']:.1f},{controls['ref_vento']:.1f}\n"
                
                # Write to serial
                if self.serial_conn and self.serial_conn.is_open:
                    self.serial_conn.write(cmd_string.encode('utf-8'))
                    self.serial_conn.flush()
                
                # 3. Read response from Arduino (blocks until newline or timeout)
                if self.serial_conn and self.serial_conn.is_open:
                    line = self.serial_conn.readline().decode('utf-8').strip()
                    if line:
                        # Write raw line to file for debugging
                        try:
                            with open("serial_rx.log", "w", encoding="utf-8") as debug_f:
                                debug_f.write(line + "\n")
                        except Exception:
                            pass
                        parts = []
                        for x in line.split(','):
                            try:
                                parts.append(float(x))
                            except ValueError:
                                parts.append(0.0)  # Safe fallback for 'ovf', 'nan', or garbage data
                                
                        if len(parts) >= 8:
                            sensor_data = {
                                "H2_Level": parts[0],
                                "Pot_cc": parts[1],
                                "Pot_total": parts[2],
                                "SoC": parts[3],
                                "Pot_wt": parts[4],
                                "Pot_pv": parts[5],
                                "Pc_actual": parts[6],
                                "Pd_actual": parts[7]
                            }
                            # Push to queue (overwriting old unread elements to prevent latency build-up)
                            if self.sensor_data_queue.full():
                                try: self.sensor_data_queue.get_nowait()
                                except Empty: pass
                            self.sensor_data_queue.put_nowait(sensor_data)
            except Exception as e:
                print(f"[Serial] Physical communication error: {e}")
                self.is_connected = False
                self.running = False
                break
                
            time.sleep(self.sleep_time)


