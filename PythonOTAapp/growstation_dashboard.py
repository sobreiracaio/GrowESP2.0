#!/usr/bin/env python3
"""
GrowStation Dashboard
- Lê usuários diretamente do Firebase (qualquer email cadastrado)
- Exibe gráficos de sensores com marcações de atuadores
- Atualiza a cada 5 minutos
- Salva histórico em CSV por usuário
"""

import tkinter as tk
from tkinter import ttk, messagebox
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
import requests
import json
import threading
import time
import datetime
import csv
import os
from collections import defaultdict

# ── Configurações Firebase ────────────────────────────────────────────────────
API_KEY      = "AIzaSyBZOxkbUT3b9COdhCqNul3kNy6HEuSU5S4"
DATABASE_URL = "https://growstation-183df-default-rtdb.firebaseio.com"
AUTH_EMAIL   = "caiosobreira@gmail.com"
AUTH_PASS    = "growstation"  # altere para a senha real

UPDATE_INTERVAL = 300  # segundos (5 min)
CSV_DIR = "growstation_data"

# ── Cores e estilo ────────────────────────────────────────────────────────────
BG       = "#1a1a2e"
CARD_BG  = "#16213e"
TEXT     = "#e0e0e0"
ACCENT   = "#0f3460"
GREEN    = "#1ea896"
YELLOW   = "#ddb967"
RED      = "#ff715b"
BLUE     = "#2176ae"
PURPLE   = "#6a0572"
ORANGE   = "#e07b39"

SENSOR_COLORS = {
    "Temperature": RED,
    "Humidity":    BLUE,
    "Soil":        GREEN,
    "WaterReserv": PURPLE,
}

ACTUATOR_COLORS = {
    "LightStatus":   YELLOW,
    "PumpStatus":    BLUE,
    "CoolerStatus":  GREEN,
    "HeaterStatus":  RED,
    "HumidStatus":   PURPLE,
    "DehumidStatus": ORANGE,
}

SENSOR_LABELS = {
    "Temperature": "Temperatura (°C)",
    "Humidity":    "Umidade (%)",
    "Soil":        "Solo (%)",
    "WaterReserv": "Reservatório (%)",
}

# ── Firebase Auth ─────────────────────────────────────────────────────────────
class FirebaseAuth:
    def __init__(self):
        self.id_token = None
        self.refresh_token = None
        self.expiry = 0

    def authenticate(self, email, password):
        url = f"https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key={API_KEY}"
        r = requests.post(url, json={
            "email": email,
            "password": password,
            "returnSecureToken": True
        }, timeout=15)
        if r.status_code == 200:
            data = r.json()
            self.id_token     = data["idToken"]
            self.refresh_token = data["refreshToken"]
            self.expiry       = time.time() + int(data["expiresIn"]) - 60
            return True
        return False

    def ensure_token(self):
        if not self.id_token or time.time() >= self.expiry:
            return self.authenticate(AUTH_EMAIL, AUTH_PASS)
        return True

    def get(self, path):
        if not self.ensure_token():
            return None
        url = f"{DATABASE_URL}/{path}.json?auth={self.id_token}"
        r = requests.get(url, timeout=10)
        if r.status_code == 200:
            return r.json()
        return None

# ── CSV ───────────────────────────────────────────────────────────────────────
def ensure_csv_dir():
    os.makedirs(CSV_DIR, exist_ok=True)

def save_to_csv(safe_email, timestamp, sensors, actuators):
    ensure_csv_dir()
    path = os.path.join(CSV_DIR, f"{safe_email}.csv")
    exists = os.path.exists(path)
    with open(path, "a", newline="") as f:
        writer = csv.writer(f)
        if not exists:
            writer.writerow([
                "timestamp",
                "Temperature", "Humidity", "Soil", "WaterReserv",
                "LightStatus", "PumpStatus", "CoolerStatus",
                "HeaterStatus", "HumidStatus", "DehumidStatus"
            ])
        writer.writerow([
            timestamp.isoformat(),
            sensors.get("Temperature", ""),
            sensors.get("Humidity", ""),
            sensors.get("Soil", ""),
            sensors.get("WaterReserv", ""),
            actuators.get("LightStatus", ""),
            actuators.get("PumpStatus", ""),
            actuators.get("CoolerStatus", ""),
            actuators.get("HeaterStatus", ""),
            actuators.get("HumidStatus", ""),
            actuators.get("DehumidStatus", ""),
        ])

def load_from_csv(safe_email):
    path = os.path.join(CSV_DIR, f"{safe_email}.csv")
    if not os.path.exists(path):
        return [], {k: [] for k in SENSOR_LABELS}, {k: [] for k in ACTUATOR_COLORS}
    timestamps = []
    sensors   = defaultdict(list)
    actuators = defaultdict(list)
    with open(path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                timestamps.append(datetime.datetime.fromisoformat(row["timestamp"]))
                for s in SENSOR_LABELS:
                    v = row.get(s, "")
                    sensors[s].append(float(v) if v != "" else None)
                for a in ACTUATOR_COLORS:
                    v = row.get(a, "")
                    actuators[a].append(v.lower() == "true" if v != "" else None)
            except Exception:
                pass
    return timestamps, dict(sensors), dict(actuators)

# ── Login Dialog ──────────────────────────────────────────────────────────────
class LoginDialog(tk.Toplevel):
    def __init__(self, parent):
        super().__init__(parent)
        self.title("GrowStation — Login")
        self.configure(bg=BG)
        self.resizable(False, False)
        self.result = None

        tk.Label(self, text="🌿 GrowStation Dashboard",
                 bg=BG, fg=GREEN, font=("Helvetica", 16, "bold")).pack(pady=(20, 5))
        tk.Label(self, text=f"Admin: {AUTH_EMAIL}",
                 bg=BG, fg=TEXT, font=("Helvetica", 9)).pack(pady=(0, 15))

        frame = tk.Frame(self, bg=BG)
        frame.pack(padx=30, pady=5)

        tk.Label(frame, text="Senha:", bg=BG, fg=TEXT).grid(row=0, column=0, sticky="w", pady=5)
        self.pass_var = tk.StringVar()
        pw = tk.Entry(frame, textvariable=self.pass_var, show="*", width=25,
                      bg=CARD_BG, fg=TEXT, insertbackground=TEXT)
        pw.grid(row=0, column=1, pady=5, padx=5)
        pw.focus_set()
        self.bind("<Return>", lambda e: self.login())

        self.status = tk.Label(self, text="", bg=BG, fg=RED, font=("Helvetica", 9))
        self.status.pack()

        btn = tk.Button(self, text="Entrar", command=self.login,
                        bg=GREEN, fg="white", font=("Helvetica", 11, "bold"),
                        relief="flat", padx=20, pady=8, cursor="hand2")
        btn.pack(pady=15)

        self.grab_set()
        self.protocol("WM_DELETE_WINDOW", self.destroy)
        self.center()

    def center(self):
        self.update_idletasks()
        x = (self.winfo_screenwidth()  - self.winfo_width())  // 2
        y = (self.winfo_screenheight() - self.winfo_height()) // 2
        self.geometry(f"+{x}+{y}")

    def login(self):
        self.status.config(text="Conectando...", fg=YELLOW)
        self.update()
        auth = FirebaseAuth()
        if auth.authenticate(AUTH_EMAIL, self.pass_var.get()):
            self.result = auth
            self.destroy()
        else:
            self.status.config(text="❌ Email ou senha inválidos", fg=RED)

# ── User Tab ──────────────────────────────────────────────────────────────────
class UserTab(tk.Frame):
    def __init__(self, parent, safe_email, display_email, auth):
        super().__init__(parent, bg=BG)
        self.safe_email    = safe_email
        self.display_email = display_email
        self.auth          = auth

        self.timestamps = []
        self.sensors    = {k: [] for k in SENSOR_LABELS}
        self.actuators  = {k: [] for k in ACTUATOR_COLORS}
        self.prev_act   = {k: None for k in ACTUATOR_COLORS}
        self.events     = []  # (timestamp, actuator, state)

        # Carrega histórico CSV
        ts, s, a = load_from_csv(safe_email)
        if ts:
            self.timestamps = ts
            self.sensors    = s
            self.actuators  = a
            # reconstrói eventos do histórico
            for i, t in enumerate(ts):
                for act in ACTUATOR_COLORS:
                    cur = a[act][i]
                    prev = a[act][i-1] if i > 0 else None
                    if cur is not None and cur != prev:
                        self.events.append((t, act, cur))

        self._build_ui()

    def _build_ui(self):
        # Status bar
        top = tk.Frame(self, bg=CARD_BG)
        top.pack(fill="x", padx=10, pady=(10, 0))

        tk.Label(top, text=f"📧 {self.display_email}",
                 bg=CARD_BG, fg=GREEN, font=("Helvetica", 11, "bold")).pack(side="left", padx=10, pady=8)

        self.status_lbl = tk.Label(top, text="Aguardando dados...",
                                    bg=CARD_BG, fg=YELLOW, font=("Helvetica", 9))
        self.status_lbl.pack(side="left", padx=10)

        self.last_lbl = tk.Label(top, text="",
                                  bg=CARD_BG, fg=TEXT, font=("Helvetica", 9))
        self.last_lbl.pack(side="right", padx=10)

        # Sensor cards
        cards = tk.Frame(self, bg=BG)
        cards.pack(fill="x", padx=10, pady=5)
        self.sensor_lbls = {}
        for i, (key, label) in enumerate(SENSOR_LABELS.items()):
            c = tk.Frame(cards, bg=CARD_BG, relief="flat")
            c.grid(row=0, column=i, padx=5, pady=5, sticky="ew")
            cards.columnconfigure(i, weight=1)
            tk.Label(c, text=label.split("(")[0].strip(),
                     bg=CARD_BG, fg=TEXT, font=("Helvetica", 9)).pack(pady=(8,0))
            lbl = tk.Label(c, text="--",
                           bg=CARD_BG, fg=SENSOR_COLORS[key],
                           font=("Helvetica", 18, "bold"))
            lbl.pack(pady=(2, 8))
            self.sensor_lbls[key] = lbl

        # Actuator cards
        act_frame = tk.Frame(self, bg=BG)
        act_frame.pack(fill="x", padx=10, pady=2)
        self.act_lbls = {}
        act_names = {
            "LightStatus": "Luz",
            "PumpStatus": "Bomba",
            "CoolerStatus": "Cooler",
            "HeaterStatus": "Aquecedor",
            "HumidStatus": "Umid.",
            "DehumidStatus": "Desumid.",
        }
        for i, (key, name) in enumerate(act_names.items()):
            c = tk.Frame(act_frame, bg=CARD_BG, relief="flat")
            c.grid(row=0, column=i, padx=4, pady=3, sticky="ew")
            act_frame.columnconfigure(i, weight=1)
            tk.Label(c, text=name, bg=CARD_BG, fg=TEXT,
                     font=("Helvetica", 8)).pack(pady=(5,0))
            lbl = tk.Label(c, text="--",
                           bg=CARD_BG, fg=TEXT,
                           font=("Helvetica", 10, "bold"))
            lbl.pack(pady=(0, 5))
            self.act_lbls[key] = lbl

        # Gráficos
        fig_frame = tk.Frame(self, bg=BG)
        fig_frame.pack(fill="both", expand=True, padx=10, pady=5)

        self.fig = Figure(figsize=(12, 5), facecolor=BG, tight_layout=True)
        self.axes = {}
        sensor_keys = list(SENSOR_LABELS.keys())
        for i, key in enumerate(sensor_keys):
            ax = self.fig.add_subplot(2, 2, i+1)
            ax.set_facecolor(CARD_BG)
            ax.tick_params(colors=TEXT, labelsize=7)
            ax.xaxis.label.set_color(TEXT)
            ax.yaxis.label.set_color(TEXT)
            ax.set_title(SENSOR_LABELS[key], color=TEXT, fontsize=8, pad=4)
            for spine in ax.spines.values():
                spine.set_edgecolor(ACCENT)
            self.axes[key] = ax

        self.canvas = FigureCanvasTkAgg(self.fig, master=fig_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        if self.timestamps:
            self.redraw()

    def update_data(self, sensors, actuators, targets, tolerances, ts):
        self.timestamps.append(ts)
        self.targets    = targets
        self.tolerances = tolerances

        for key, val in actuators.items():
            prev = self.prev_act[key]
            if val is not None and val != prev:
                self.events.append((ts, key, val))
            self.prev_act[key] = val
            self.actuators[key].append(val)

        for key, val in sensors.items():
            self.sensors[key].append(val)

        for key, lbl in self.sensor_lbls.items():
            v = sensors.get(key)
            if v is not None:
                unit = "°C" if key == "Temperature" else "%"
                lbl.config(text=f"{v:.1f}{unit}")

        for key, lbl in self.act_lbls.items():
            v = actuators.get(key)
            if v is not None:
                lbl.config(
                    text="ON" if v else "OFF",
                    fg=ACTUATOR_COLORS[key] if v else TEXT
                )

        self.last_lbl.config(text=f"Atualizado: {ts.strftime('%H:%M:%S')}")
        self.status_lbl.config(text="✅ Online", fg=GREEN)

        save_to_csv(self.safe_email, ts, sensors, actuators)
        self.redraw()

    def redraw(self):
        if len(self.timestamps) < 1:
            return

        # Y caps fixos por sensor
        Y_MAX = {
            "Temperature": 50,
            "Humidity":    100,
            "Soil":        100,
            "WaterReserv": 100,
        }

        # Limita a 288 pontos (24h com intervalo de 5min)
        max_pts = 288
        ts   = self.timestamps[-max_pts:]
        evts = [(t, a, s) for t, a, s in self.events if t >= ts[0]]

        targets    = getattr(self, "targets",    {})
        tolerances = getattr(self, "tolerances", {})

        act_map = {
            "Temperature": ["CoolerStatus", "HeaterStatus"],
            "Humidity":    ["HumidStatus", "DehumidStatus"],
            "Soil":        ["PumpStatus"],
            "WaterReserv": ["PumpStatus"],
        }

        for key, ax in self.axes.items():
            ax.cla()
            ax.set_facecolor(CARD_BG)
            ax.tick_params(colors=TEXT, labelsize=7)
            ax.set_title(SENSOR_LABELS[key], color=TEXT, fontsize=8, pad=4)
            for spine in ax.spines.values():
                spine.set_edgecolor(ACCENT)

            # Y fixo
            ymax = Y_MAX.get(key, 100)
            ax.set_ylim(0, ymax)

            vals  = self.sensors[key][-max_pts:]
            valid = [(t, v) for t, v in zip(ts, vals) if v is not None]
            if len(valid) == 1:
                ax.scatter([valid[0][0]], [valid[0][1]],
                           color=SENSOR_COLORS[key], s=30, zorder=2)
            elif len(valid) >= 2:
                vts, vvals = zip(*valid)
                ax.plot(list(vts), list(vvals),
                        color=SENSOR_COLORS[key], linewidth=1.5, zorder=2)
                ax.fill_between(list(vts), list(vvals),
                                alpha=0.1, color=SENSOR_COLORS[key], zorder=1)

            # Linhas de target e banda de tolerância
            tgt = targets.get(key)
            tol = tolerances.get(key)
            if tgt is not None:
                ax.axhline(y=tgt, color=YELLOW, linewidth=1,
                           linestyle="--", alpha=0.85, zorder=4)
                ax.text(0.01, tgt / ymax + 0.02,
                        f"Alvo: {tgt}",
                        transform=ax.transAxes,
                        color=YELLOW, fontsize=6, va="bottom")
                if tol is not None:
                    lo = max(tgt - tol, 0)
                    hi = min(tgt + tol, ymax)
                    ax.axhline(y=hi, color=YELLOW, linewidth=0.7,
                               linestyle=":", alpha=0.5, zorder=4)
                    ax.axhline(y=lo, color=YELLOW, linewidth=0.7,
                               linestyle=":", alpha=0.5, zorder=4)
                    ax.axhspan(lo, hi, alpha=0.06, color=YELLOW, zorder=1)
                    ax.text(0.01, hi / ymax + 0.01,
                            f"+{tol}", transform=ax.transAxes,
                            color=YELLOW, fontsize=5, alpha=0.8)
                    ax.text(0.01, max(lo / ymax - 0.05, 0),
                            f"-{tol}", transform=ax.transAxes,
                            color=YELLOW, fontsize=5, alpha=0.8)

            # Marcações de atuadores
            for t, act, state in evts:
                if act in act_map.get(key, []):
                    color = ACTUATOR_COLORS[act]
                    ax.axvline(x=t, color=color,
                               linestyle="--" if state else ":",
                               alpha=0.7, linewidth=1, zorder=3)
                    label = act.replace("Status", "")
                    ax.annotate(
                        f"{'▲' if state else '▼'}{label}",
                        xy=(t, ymax * 0.95),
                        fontsize=6, color=color, rotation=90,
                        xytext=(2, -4), textcoords="offset points"
                    )

            ax.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
            ax.xaxis.set_major_locator(mdates.AutoDateLocator())
            self.fig.autofmt_xdate(rotation=30, ha="right")

        self.canvas.draw_idle()

    def set_error(self, msg):
        self.status_lbl.config(text=f"⚠️ {msg}", fg=RED)

# ── Main App ──────────────────────────────────────────────────────────────────
class GrowDashboard(tk.Tk):
    def __init__(self, auth):
        super().__init__()
        self.auth = auth
        self.title("🌿 GrowStation Dashboard")
        self.configure(bg=BG)
        self.geometry("1100x720")
        self.minsize(900, 600)

        self.user_tabs = {}
        self._build_ui()
        self._load_users()
        self._schedule_update()

    def _build_ui(self):
        # Header
        header = tk.Frame(self, bg=CARD_BG, height=50)
        header.pack(fill="x")
        header.pack_propagate(False)

        tk.Label(header, text="🌿 GrowStation Dashboard",
                 bg=CARD_BG, fg=GREEN,
                 font=("Helvetica", 14, "bold")).pack(side="left", padx=15, pady=10)

        self.countdown_lbl = tk.Label(header, text="",
                                       bg=CARD_BG, fg=YELLOW,
                                       font=("Helvetica", 9))
        self.countdown_lbl.pack(side="right", padx=15)

        btn_refresh = tk.Button(header, text="⟳ Atualizar Agora",
                                 command=self._fetch_all,
                                 bg=ACCENT, fg=TEXT,
                                 font=("Helvetica", 9),
                                 relief="flat", padx=10, pady=5,
                                 cursor="hand2")
        btn_refresh.pack(side="right", padx=5, pady=8)

        # Notebook
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TNotebook",        background=BG,      borderwidth=0)
        style.configure("TNotebook.Tab",    background=CARD_BG, foreground=TEXT,
                        padding=[12, 6],    font=("Helvetica", 9))
        style.map("TNotebook.Tab",
                  background=[("selected", ACCENT)],
                  foreground=[("selected", GREEN)])

        self.notebook = ttk.Notebook(self)
        self.notebook.pack(fill="both", expand=True, padx=5, pady=5)

        # Status bar
        self.status_bar = tk.Label(self, text="Iniciando...",
                                    bg=CARD_BG, fg=TEXT,
                                    font=("Helvetica", 8), anchor="w")
        self.status_bar.pack(fill="x", side="bottom")

    def _load_users(self):
        """Busca usuários do Firebase.
        Tenta ler a raiz primeiro. Se falhar (regras do banco), usa
        shallow=true para listar apenas as chaves, e valida cada uma.
        Fallback final: lista de emails conhecidos hardcoded.
        """
        self.status_bar.config(text="Buscando usuários no Firebase...")
        self.update()

        # Lista de fallback — adicione mais emails conforme necessário
        KNOWN_EMAILS = [
            "caiosobreira@gmail.com",
            "robgandra@gmail.com",
        ]

        def safe_email_from(email):
            """caiosobreira@gmail.com → caiosobreira@gmail_com"""
            return email.replace(".", "_")

        users_found = 0

        # Tentativa 1: GET raiz com shallow=true (lista só as chaves, sem dados)
        try:
            if not self.auth.ensure_token():
                raise Exception("Token inválido")
            url = f"{DATABASE_URL}/.json?shallow=true&auth={self.auth.id_token}"
            r = requests.get(url, timeout=10)
            if r.status_code == 200:
                keys = r.json()
                if isinstance(keys, dict):
                    for key in keys:
                        # Ignora nós globais que começam com _
                        if key.startswith("_"):
                            continue
                        # Busca o campo email do nó
                        node = self.auth.get(f"{key}/email")
                        if isinstance(node, str) and "@" in node:
                            self._add_user_tab(key, node)
                            users_found += 1
        except Exception:
            pass

        # Tentativa 2: fallback para lista conhecida
        if users_found == 0:
            for email in KNOWN_EMAILS:
                safe = safe_email_from(email)
                # Verifica se o nó existe
                try:
                    node = self.auth.get(f"{safe}/email")
                    if isinstance(node, str):
                        self._add_user_tab(safe, node)
                        users_found += 1
                except Exception:
                    pass

        if users_found == 0:
            self.status_bar.config(text="Nenhum usuário encontrado no Firebase.")
            messagebox.showwarning("Aviso", "Nenhum usuário encontrado.\nVerifique as credenciais e as regras do banco.")
        else:
            self.status_bar.config(text=f"{users_found} usuário(s) carregado(s). Buscando dados...")
            self._fetch_all()

    def _add_user_tab(self, safe_email, display_email):
        tab = UserTab(self.notebook, safe_email, display_email, self.auth)
        self.notebook.add(tab, text=f"  {display_email}  ")
        self.user_tabs[safe_email] = tab

    def _fetch_all(self):
        threading.Thread(target=self._fetch_all_thread, daemon=True).start()

    def _fetch_all_thread(self):
        now = datetime.datetime.now()
        self.status_bar.config(text=f"Atualizando dados... {now.strftime('%H:%M:%S')}")

        for safe_email, tab in self.user_tabs.items():
            try:
                readings = self.auth.get(f"{safe_email}/Readings")
                if not readings:
                    tab.set_error("Sem dados de leitura")
                    continue

                sensors = {
                    "Temperature": readings.get("Sensor", {}).get("Temperature"),
                    "Humidity":    readings.get("Sensor", {}).get("Humidity"),
                    "Soil":        readings.get("Sensor", {}).get("Soil"),
                    "WaterReserv": readings.get("Sensor", {}).get("WaterReserv"),
                }
                actuators = {
                    "LightStatus":   readings.get("Actuator", {}).get("LightStatus"),
                    "PumpStatus":    readings.get("Actuator", {}).get("PumpStatus"),
                    "CoolerStatus":  readings.get("Actuator", {}).get("CoolerStatus"),
                    "HeaterStatus":  readings.get("Actuator", {}).get("HeaterStatus"),
                    "HumidStatus":   readings.get("Actuator", {}).get("HumidStatus"),
                    "DehumidStatus": readings.get("Actuator", {}).get("DehumidStatus"),
                }

                # Busca targets e tolerancias do InsertedData
                inserted = self.auth.get(f"{safe_email}/InsertedData/Sensor") or {}
                targets = {
                    "Temperature": inserted.get("Temperature", {}).get("TargetTemp"),
                    "Humidity":    inserted.get("Humid",       {}).get("TargetHumid"),
                    "Soil":        inserted.get("Soil",        {}).get("TargetSoil"),
                    "WaterReserv": None,
                }
                tolerances = {
                    "Temperature": inserted.get("Temperature", {}).get("TempTolerance"),
                    "Humidity":    inserted.get("Humid",       {}).get("HumidTolerance"),
                    "Soil":        inserted.get("Soil",        {}).get("SoilTolerance"),
                    "WaterReserv": None,
                }

                self.after(0, tab.update_data, sensors, actuators, targets, tolerances, now)

            except Exception as e:
                self.after(0, tab.set_error, str(e))

        self.after(0, lambda: self.status_bar.config(
            text=f"Última atualização: {now.strftime('%d/%m/%Y %H:%M:%S')}  |  "
                 f"Próxima em {UPDATE_INTERVAL // 60} min"
        ))

    def _schedule_update(self):
        self._next_update = time.time() + UPDATE_INTERVAL
        self._tick()

    def _tick(self):
        remaining = int(self._next_update - time.time())
        if remaining <= 0:
            self._next_update = time.time() + UPDATE_INTERVAL
            self._fetch_all()
        m, s = divmod(max(remaining, 0), 60)
        self.countdown_lbl.config(text=f"Próxima atualização: {m:02d}:{s:02d}")
        self.after(1000, self._tick)


# ── Entry Point ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    root = tk.Tk()
    root.withdraw()  # esconde janela principal durante login

    login = LoginDialog(root)
    root.wait_window(login)

    if login.result is None:
        root.destroy()
    else:
        root.destroy()
        app = GrowDashboard(login.result)
        app.mainloop()