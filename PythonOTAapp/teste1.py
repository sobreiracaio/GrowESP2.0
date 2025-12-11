import tkinter as tk
from tkinter import ttk, messagebox
import firebase_admin
from firebase_admin import credentials, db
import threading

# ==============================
# CONFIGURAÇÃO FIREBASE
# ==============================
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://growstation-183df-default-rtdb.firebaseio.com"
})

# ==============================
# TELA DE LOGIN
# ==============================
class LoginScreen:
    def __init__(self, root):
        self.root = root
        self.root.title("Login Growstation")
        self.root.geometry("300x200")

        ttk.Label(root, text="🌿 Growstation", font=("Arial", 16, "bold")).pack(pady=10)

        ttk.Label(root, text="Email:").pack(pady=2)
        self.email_entry = ttk.Entry(root, width=30)
        self.email_entry.pack()

        ttk.Label(root, text="Senha:").pack(pady=2)
        self.pass_entry = ttk.Entry(root, width=30, show="*")
        self.pass_entry.pack()

        ttk.Button(root, text="Entrar", command=self.try_login).pack(pady=15)

    def try_login(self):
        email = self.email_entry.get().strip()
        password = self.pass_entry.get().strip()

        if not email or not password:
            messagebox.showerror("Erro", "Preencha email e senha.")
            return

        safe_email = email.replace(".", "_")

        try:
            ref = db.reference(safe_email)
            data = ref.get()

            if not data:
                messagebox.showerror("Erro", "Usuário não encontrado.")
                return

            stored_pass = data.get("Pass")
            if stored_pass != password:
                messagebox.showerror("Erro", "Senha incorreta.")
                return

            self.root.destroy()
            main_root = tk.Tk()
            FirebaseApp(main_root, safe_email)
            main_root.mainloop()

        except Exception as e:
            messagebox.showerror("Erro Firebase", str(e))

# ==============================
# CLASSE PRINCIPAL
# ==============================
class FirebaseApp:
    def __init__(self, root, safe_email):
        self.root = root
        self.root.title("🌿 Growstation Firebase")
        self.root.geometry("320x720")
        self.user_path = f"{safe_email}/"

        # Scroll
        container = ttk.Frame(root)
        container.pack(fill="both", expand=True)
        canvas = tk.Canvas(container)
        scrollbar = ttk.Scrollbar(container, orient="vertical", command=canvas.yview)
        self.scrollable_frame = ttk.Frame(canvas)
        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        ttk.Label(self.scrollable_frame, text="Growstation Monitor", font=("Arial", 16, "bold")).pack(pady=10)

        # ==============================
        # ATUADORES
        # ==============================
        self.frame_actuator = ttk.LabelFrame(self.scrollable_frame, text="⚙️ Atuadores")
        self.frame_actuator.pack(padx=10, pady=5, fill="x")
        self.actuator_labels = {}
        for i, name in enumerate(["Dehumid", "Fan", "Humid", "Light", "Pump"]):
            ttk.Label(self.frame_actuator, text=f"{name}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            lbl = ttk.Label(self.frame_actuator, text="---", width=10)
            lbl.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.actuator_labels[name] = lbl

        # ==============================
        # SENSORES
        # ==============================
        self.frame_sensor = ttk.LabelFrame(self.scrollable_frame, text="📊 Sensores")
        self.frame_sensor.pack(padx=10, pady=5, fill="x")
        self.sensor_labels = {}
        for i, name in enumerate(["Humidity", "Soil", "Temperature"]):
            ttk.Label(self.frame_sensor, text=f"{name}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            lbl = ttk.Label(self.frame_sensor, text="---", width=10)
            lbl.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.sensor_labels[name] = lbl

        # ==============================
        # TARGETS
        # ==============================
        self.frame_targets = ttk.LabelFrame(self.scrollable_frame, text="🎯 Alvos")
        self.frame_targets.pack(padx=10, pady=5, fill="x")
        vcmd_int = (self.root.register(lambda P: P.isdigit() or P == ""), "%P")
        self.target_inputs = {}
        for i, name in enumerate(["TargetHumid", "TargetSoil", "TargetTemp"]):
            ttk.Label(self.frame_targets, text=f"{name}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            entry = ttk.Entry(self.frame_targets, width=6, validate="key", validatecommand=vcmd_int)
            entry.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.target_inputs[name] = entry
        ttk.Button(self.frame_targets, text="💾 Gravar Targets", command=self.save_targets).grid(row=3, column=0, columnspan=2, pady=10)

        # ==============================
        # CONFIGURAÇÕES DE SENSOR
        # ==============================
        self.frame_sensor_settings = ttk.LabelFrame(self.scrollable_frame, text="⚙️ Configurações Sensores")
        self.frame_sensor_settings.pack(padx=10, pady=5, fill="x")
        vcmd_float = (self.root.register(lambda P: P.replace(".", "").isdigit() or P == ""), "%P")
        self.sensor_settings_inputs = {}
        sensor_keys = [
            ("TargetHumidTolerance", "Humid Tolerance"),
            ("TargetTempTolerance", "Temp Tolerance"),
            ("TargetSoilPumpDuration", "Soil Pump Duration"),
            ("TargetSoilAbsorptionDelay", "Soil Absorption Delay"),
            ("TargetSoilTolerance", "Soil Tolerance")
        ]
        for i, (key, label) in enumerate(sensor_keys):
            ttk.Label(self.frame_sensor_settings, text=f"{label}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            entry = ttk.Entry(self.frame_sensor_settings, width=6, validate="key", validatecommand=vcmd_float)
            entry.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.sensor_settings_inputs[key] = entry
        ttk.Button(self.frame_sensor_settings, text="💾 Gravar Configurações", command=self.save_sensor_settings).grid(row=len(sensor_keys), column=0, columnspan=2, pady=10)

        # ==============================
        # HORÁRIOS
        # ==============================
        self.frame_write = ttk.LabelFrame(self.scrollable_frame, text="🕒 Horário da Luz")
        self.frame_write.pack(padx=10, pady=5, fill="x")
        ttk.Label(self.frame_write, text="Hora ON:").grid(row=0, column=0, padx=5, pady=5, sticky="e")
        self.hour_on_hh = ttk.Entry(self.frame_write, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_on_mm = ttk.Entry(self.frame_write, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_on_hh.grid(row=0, column=1)
        ttk.Label(self.frame_write, text=":").grid(row=0, column=2)
        self.hour_on_mm.grid(row=0, column=3)
        ttk.Label(self.frame_write, text="Hora OFF:").grid(row=1, column=0, padx=5, pady=5, sticky="e")
        self.hour_off_hh = ttk.Entry(self.frame_write, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_off_mm = ttk.Entry(self.frame_write, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_off_hh.grid(row=1, column=1)
        ttk.Label(self.frame_write, text=":").grid(row=1, column=2)
        self.hour_off_mm.grid(row=1, column=3)
        ttk.Button(self.frame_write, text="💾 Gravar Horário", command=self.save_times).grid(row=2, column=0, columnspan=4, pady=10)

        # Status
        self.status_label = ttk.Label(self.scrollable_frame, text="", foreground="gray")
        self.status_label.pack(pady=5)

        # Carrega dados iniciais
        self.load_initial_data()
        self.update_data()

    # ==============================
    # FUNÇÕES AUXILIARES
    # ==============================
    def get_ref(self, path): 
        return db.reference(self.user_path + path)

    def load_initial_data(self):
        self.load_targets()
        self.load_sensor_settings()
        self.load_times()

    def load_targets(self):
        data = self.get_ref("InsertedData/Sensor").get()
        if data:
            for key in ["TargetHumid", "TargetSoil", "TargetTemp"]:
                if key in data:
                    self.target_inputs[key].delete(0, tk.END)
                    self.target_inputs[key].insert(0, str(data[key]))

    def load_sensor_settings(self):
        data = self.get_ref("InsertedData/Sensor").get()
        if data:
            for key in self.sensor_settings_inputs.keys():
                if key in data:
                    self.sensor_settings_inputs[key].delete(0, tk.END)
                    self.sensor_settings_inputs[key].insert(0, str(data[key]))

    def load_times(self):
        data = self.get_ref("InsertedData/Light").get()
        if data:
            for key, hh_entry, mm_entry in [
                ("HourOn", self.hour_on_hh, self.hour_on_mm),
                ("HourOff", self.hour_off_hh, self.hour_off_mm)
            ]:
                value = data.get(key, "00:00")
                hh, mm = value.split(":")
                hh_entry.delete(0, tk.END)
                hh_entry.insert(0, hh)
                mm_entry.delete(0, tk.END)
                mm_entry.insert(0, mm)

    # ==============================
    # ATUALIZAÇÃO AUTOMÁTICA
    # ==============================
    def update_data(self):
        threading.Thread(target=self._refresh, daemon=True).start()
        self.root.after(5000, self.update_data)

    def _refresh(self):
        try:
            actuators = self.get_ref("Actuator").get()
            sensors = self.get_ref("Sensor").get()
            if actuators:
                for key, lbl in self.actuator_labels.items():
                    state = actuators.get(key, False)
                    lbl.config(text="ON" if state else "OFF", foreground="green" if state else "red")
            if sensors:
                for key in ["Humidity", "Soil", "Temperature"]:
                    val = sensors.get(key, "---")
                    self.sensor_labels[key].config(text=str(val))
        except Exception as e:
            self.status_label.config(text=f"Erro: {e}")

    # ==============================
    # SALVAR DADOS
    # ==============================
    def save_targets(self):
        try:
            data = {k: v.get() for k, v in self.target_inputs.items()}
            self.get_ref("InsertedData/Sensor").update(data)
            messagebox.showinfo("Sucesso", "Targets gravados!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_sensor_settings(self):
        try:
            for key, entry in self.sensor_settings_inputs.items():
                value = entry.get()
                if value:
                    self.get_ref("InsertedData/Sensor/" + key).set(float(value))
            messagebox.showinfo("Sucesso", "Configurações gravadas!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_times(self):
        try:
            hh_on, mm_on = self.hour_on_hh.get(), self.hour_on_mm.get()
            hh_off, mm_off = self.hour_off_hh.get(), self.hour_off_mm.get()
            if not all(x.isdigit() for x in [hh_on, mm_on, hh_off, mm_off]):
                return messagebox.showerror("Erro", "Somente números permitidos.")
            if not (0 <= int(hh_on) <= 23 and 0 <= int(mm_on) <= 59 and 0 <= int(hh_off) <= 23 and 0 <= int(mm_off) <= 59):
                return messagebox.showerror("Erro", "Hora inválida (00–23 / 00–59).")

            self.get_ref("InsertedData/Light").update({
                "HourOn": f"{hh_on.zfill(2)}:{mm_on.zfill(2)}",
                "HourOff": f"{hh_off.zfill(2)}:{mm_off.zfill(2)}"
            })
            messagebox.showinfo("Sucesso", "Horários gravados!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

# ==============================
# MAIN
# ==============================
if __name__ == "__main__":
    root = tk.Tk()
    LoginScreen(root)
    root.mainloop()