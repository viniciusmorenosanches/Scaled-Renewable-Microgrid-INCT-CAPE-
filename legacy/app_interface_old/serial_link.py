import serial
import time
import logging

class MicrogridSerialLink:
    """
    Ponte de comunicação Serial entre o PC (Streamlit/MPC) e o Arduino Mestre (R4).
    """
    def __init__(self, port, baudrate=9600, timeout=1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        
    def conectar(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=self.timeout)
            time.sleep(2) # Aguarda o Arduino reiniciar
            logging.info(f"Conectado ao Arduino na porta {self.port}")
            return True
        except serial.SerialException as e:
            logging.error(f"Erro ao conectar na porta {self.port}: {e}")
            return False

    def desconectar(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            logging.info("Conexão Serial fechada.")

    def ler_sensores(self):
        if not self.ser or not self.ser.is_open:
            return None

        try:
            # Como o Arduino agora só envia 1 pacote em resposta ao nosso comando,
            # basta ler essa linha exata, sem deitar lixo fora.
            linha = self.ser.readline().decode('utf-8', errors='ignore').strip()
            valores = linha.split(',')
            
            if len(valores) >= 8:
                return {
                    "H2_Level": float(valores[0]),
                    "Pot_cc": float(valores[1]),
                    "Pot_total": float(valores[2]),
                    "SoC": float(valores[3]),
                    "Pot_wt": float(valores[4]),
                    "Pot_pv": float(valores[5]),
                    "Pc_medido": float(valores[6]),
                    "Pd_medido": float(valores[7])
                }
        except Exception as e:
            logging.warning(f"Erro ao ler/decodificar Serial: {e}")
            
        return None

    def enviar_comandos(self, pc, pd, ucc, uel, ref_sol, ref_vento):
        """
        Envia comandos e clima. Formato: Pc, Pd, ucc, uel, ref_sol, ref_vento \n
        """
        if not self.ser or not self.ser.is_open:
            return False

        try:
            ucc_bin = 1 if ucc else 0
            uel_bin = 1 if uel else 0
            
            comando_str = f"{pc:.2f},{pd:.2f},{ucc_bin},{uel_bin},{ref_sol:.2f},{ref_vento:.2f}\n"
            
            self.ser.write(comando_str.encode('utf-8'))
            return True
            
        except Exception as e:
            logging.error(f"Erro ao enviar comando Serial: {e}")
            return False