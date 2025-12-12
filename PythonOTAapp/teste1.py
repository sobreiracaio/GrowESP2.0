import tkinter as tk
from tkinter import ttk, messagebox
import firebase_admin
from firebase_admin import credentials, db
import threading
import csv
import os
from datetime import datetime
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure
import matplotlib.dates as mdates

# ==============================
# CONFIGURAÇÃO FIREBASE
# ==============================
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    "databaseURL": "https://growstation-183df-default-rtdb.firebaseio.com"
})

# ==============================
# TEMA MANAGER
# ==============================
class ThemeManager:
    def __init__(self):
        self.current_theme = "dark"  # dark ou light
        
    def get_colors(self):
        if self.current_theme == "dark":
            return {
                'bg': '#1e1e1e',
                'fg': '#ffffff',
                'entry_bg': '#2d2d2d',
                'entry_fg': '#ffffff',
                'button_bg': '#3d3d3d',
                'button_hover': '#4d4d4d',
                'button_active': '#2d2d2d',
                'frame_bg': '#252525',
                'plot_bg': '#2d2d2d',
                'plot_fg': '#ffffff',
                'accent': '#4CAF50'
            }
        else:
            return {
                'bg': '#f0f0f0',
                'fg': '#000000',
                'entry_bg': '#ffffff',
                'entry_fg': '#000000',
                'button_bg': '#e0e0e0',
                'button_hover': '#d0d0d0',
                'button_active': '#c0c0c0',
                'frame_bg': '#ffffff',
                'plot_bg': '#ffffff',
                'plot_fg': '#000000',
                'accent': '#4CAF50'
            }

theme_manager = ThemeManager()

# ==============================
# BOTÃO CUSTOMIZADO
# ==============================
class RoundedButton(tk.Canvas):
    def __init__(self, parent, text, command=None, **kwargs):
        self.colors = theme_manager.get_colors()
        super().__init__(parent, width=kwargs.get('width', 150), height=kwargs.get('height', 40),
                        bg=self.colors['bg'], highlightthickness=0)
        
        self.command = command
        self.text = text
        self.is_pressed = False
        
        self.draw_button()
        self.bind('<Button-1>', self.on_click)
        self.bind('<ButtonRelease-1>', self.on_release)
        self.bind('<Enter>', self.on_enter)
        self.bind('<Leave>', self.on_leave)
        
    def draw_button(self, state='normal'):
        self.delete('all')
        
        if state == 'active':
            color = self.colors['button_active']
        elif state == 'hover':
            color = self.colors['button_hover']
        else:
            color = self.colors['button_bg']
            
        # Desenhar retângulo arredondado
        radius = 10
        width = self.winfo_reqwidth()
        height = self.winfo_reqheight()
        
        self.create_arc(0, 0, radius*2, radius*2, start=90, extent=90, fill=color, outline='')
        self.create_arc(width-radius*2, 0, width, radius*2, start=0, extent=90, fill=color, outline='')
        self.create_arc(0, height-radius*2, radius*2, height, start=180, extent=90, fill=color, outline='')
        self.create_arc(width-radius*2, height-radius*2, width, height, start=270, extent=90, fill=color, outline='')
        self.create_rectangle(radius, 0, width-radius, height, fill=color, outline='')
        self.create_rectangle(0, radius, width, height-radius, fill=color, outline='')
        
        # Texto
        self.create_text(width/2, height/2, text=self.text, fill=self.colors['fg'], 
                        font=('Arial', 10, 'bold'))
    
    def on_click(self, event):
        self.is_pressed = True
        self.draw_button('active')
        
    def on_release(self, event):
        if self.is_pressed:
            self.is_pressed = False
            self.draw_button('hover')
            if self.command:
                self.command()
                
    def on_enter(self, event):
        if not self.is_pressed:
            self.draw_button('hover')
            
    def on_leave(self, event):
        if not self.is_pressed:
            self.draw_button('normal')

# ==============================
# TELA DE LOGIN
# ==============================
class LoginScreen:
    def __init__(self, root):
        self.root = root
        self.root.title("Login Growstation")
        self.root.geometry("350x280")
        self.show_password = False
        
        colors = theme_manager.get_colors()
        self.root.configure(bg=colors['bg'])

        # Frame principal
        main_frame = tk.Frame(root, bg=colors['bg'])
        main_frame.pack(expand=True, fill='both', padx=20, pady=20)

        # Título
        title_label = tk.Label(main_frame, text="🌿 Growstation", 
                              font=("Arial", 18, "bold"), 
                              bg=colors['bg'], fg=colors['fg'])
        title_label.pack(pady=15)

        # Email
        tk.Label(main_frame, text="Email:", bg=colors['bg'], fg=colors['fg']).pack(pady=2)
        self.email_entry = tk.Entry(main_frame, width=30, bg=colors['entry_bg'], 
                                    fg=colors['entry_fg'], insertbackground=colors['fg'],
                                    relief='flat', bd=2)
        self.email_entry.pack(pady=5, ipady=5)
        self.email_entry.bind('<Return>', lambda e: self.pass_entry.focus())

        # Senha
        tk.Label(main_frame, text="Senha:", bg=colors['bg'], fg=colors['fg']).pack(pady=2)
        
        # Frame para senha e botão de mostrar
        pass_frame = tk.Frame(main_frame, bg=colors['bg'])
        pass_frame.pack(pady=5)
        
        self.pass_entry = tk.Entry(pass_frame, width=23, show="*", 
                                   bg=colors['entry_bg'], fg=colors['entry_fg'],
                                   insertbackground=colors['fg'], relief='flat', bd=2)
        self.pass_entry.pack(side='left', ipady=5)
        self.pass_entry.bind('<Return>', lambda e: self.try_login())
        
        self.show_pass_btn = RoundedButton(pass_frame, text="👁", command=self.toggle_password,
                                          width=40, height=34)
        self.show_pass_btn.pack(side='left', padx=5)

        # Botão entrar
        login_btn = RoundedButton(main_frame, text="Entrar", command=self.try_login,
                                 width=120, height=40)
        login_btn.pack(pady=15)

        # Foco inicial no email
        self.email_entry.focus()

    def toggle_password(self):
        self.show_password = not self.show_password
        self.pass_entry.config(show="" if self.show_password else "*")

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
        self.root.geometry("1400x900")
        self.user_path = f"{safe_email}/"
        self.csv_file = f"growstation_data_{safe_email}.csv"
        self.graphs_initialized = False
        self.current_canvas = None
        self.current_toolbar = None
        self.graph_update_interval = 10  # segundos
        self.current_graph_type = None
        
        colors = theme_manager.get_colors()
        self.root.configure(bg=colors['bg'])

        # Configurar estilo matplotlib
        plt.style.use('dark_background' if theme_manager.current_theme == 'dark' else 'default')

        # Frame superior para botão de tema
        top_frame = tk.Frame(root, bg=colors['bg'])
        top_frame.pack(fill='x', padx=10, pady=5)
        
        theme_btn = RoundedButton(top_frame, 
                                 text="🌙 Dark" if theme_manager.current_theme == "dark" else "☀️ Light",
                                 command=self.toggle_theme, width=100, height=30)
        theme_btn.pack(side='right')

        # Notebook para abas
        style = ttk.Style()
        self.configure_theme()
        
        self.notebook = ttk.Notebook(root)
        self.notebook.pack(fill="both", expand=True, padx=5, pady=5)

        # ABA 1: Configurações
        self.create_config_tab()
        
        # ABA 2: Gráficos
        self.create_graphs_tab()

        # Bind para detectar mudança de aba
        self.notebook.bind("<<NotebookTabChanged>>", self.on_tab_changed)

        # Carrega dados iniciais
        self.load_initial_data()
        self.update_data()

    def configure_theme(self):
        colors = theme_manager.get_colors()
        style = ttk.Style()
        
        style.theme_use('default')
        
        style.configure('TNotebook', background=colors['bg'], borderwidth=0)
        style.configure('TNotebook.Tab', background=colors['button_bg'], 
                       foreground=colors['fg'], padding=[10, 5])
        style.map('TNotebook.Tab', background=[('selected', colors['frame_bg'])])
        
        style.configure('TFrame', background=colors['bg'])
        style.configure('TLabel', background=colors['bg'], foreground=colors['fg'])
        style.configure('TLabelframe', background=colors['bg'], foreground=colors['fg'])
        style.configure('TLabelframe.Label', background=colors['bg'], foreground=colors['fg'])
        style.configure('TCheckbutton', background=colors['bg'], foreground=colors['fg'])
        style.configure('TEntry', fieldbackground=colors['entry_bg'], 
                       foreground=colors['entry_fg'])

    def toggle_theme(self):
        theme_manager.current_theme = "light" if theme_manager.current_theme == "dark" else "dark"
        messagebox.showinfo("Tema Alterado", "Reinicie o aplicativo para aplicar o novo tema.")

    # ==============================
    # ABA DE CONFIGURAÇÕES
    # ==============================
    def create_config_tab(self):
        colors = theme_manager.get_colors()
        config_frame = ttk.Frame(self.notebook)
        self.notebook.add(config_frame, text="⚙️ Configurações")

        # Scroll
        container = ttk.Frame(config_frame)
        container.pack(fill="both", expand=True)
        canvas = tk.Canvas(container, bg=colors['bg'], highlightthickness=0)
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
        # CAMPOS GLOBAIS
        # ==============================
        self.frame_global = ttk.LabelFrame(self.scrollable_frame, text="🌐 Configurações Globais")
        self.frame_global.pack(padx=10, pady=5, fill="x")

        ttk.Label(self.frame_global, text="_Binary URL:").grid(row=0, column=0, sticky="e", padx=5, pady=3)
        self.binary_entry = ttk.Entry(self.frame_global, width=50)
        self.binary_entry.grid(row=0, column=1, sticky="w", padx=5, pady=3)
        self.binary_current = ttk.Label(self.frame_global, text="Atual: ---", foreground="gray", font=("Arial", 8))
        self.binary_current.grid(row=1, column=1, sticky="w", padx=5)

        ttk.Label(self.frame_global, text="_Token:").grid(row=2, column=0, sticky="e", padx=5, pady=3)
        self.token_entry = ttk.Entry(self.frame_global, width=50, show="*")
        self.token_entry.grid(row=2, column=1, sticky="w", padx=5, pady=3)
        self.token_current = ttk.Label(self.frame_global, text="Atual: ---", foreground="gray", font=("Arial", 8))
        self.token_current.grid(row=3, column=1, sticky="w", padx=5)

        hasupdate_frame = ttk.Frame(self.frame_global)
        hasupdate_frame.grid(row=4, column=0, columnspan=2, pady=5)
        self.has_update_var = tk.BooleanVar()
        ttk.Checkbutton(hasupdate_frame, text="_HasUpdate", variable=self.has_update_var).pack(side="left")
        self.has_update_status = ttk.Label(hasupdate_frame, text="(Servidor: ---)", foreground="gray", font=("Arial", 8))
        self.has_update_status.pack(side="left", padx=10)

        needchange_frame = ttk.Frame(self.frame_global)
        needchange_frame.grid(row=5, column=0, columnspan=2, pady=5)
        self.need_change_var = tk.BooleanVar()
        ttk.Checkbutton(needchange_frame, text="needChange", variable=self.need_change_var).pack(side="left")
        self.need_change_status = ttk.Label(needchange_frame, text="(Servidor: ---)", foreground="gray", font=("Arial", 8))
        self.need_change_status.pack(side="left", padx=10)

        save_btn_frame = tk.Frame(self.frame_global, bg=colors['bg'])
        save_btn_frame.grid(row=6, column=0, columnspan=2, pady=10)
        RoundedButton(save_btn_frame, text="💾 Gravar Config Globais", 
                     command=self.save_global_config, width=200).pack()

        # ==============================
        # ATUADORES
        # ==============================
        self.frame_actuator = ttk.LabelFrame(self.scrollable_frame, text="⚙️ Status Atuadores")
        self.frame_actuator.pack(padx=10, pady=5, fill="x")
        self.actuator_labels = {}
        actuator_names = ["Cooler", "Dehumid", "Heater", "Humid", "Light", "Pump"]
        for i, name in enumerate(actuator_names):
            ttk.Label(self.frame_actuator, text=f"{name}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            lbl = ttk.Label(self.frame_actuator, text="---", width=10)
            lbl.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.actuator_labels[name] = lbl

        # ==============================
        # SENSORES
        # ==============================
        self.frame_sensor = ttk.LabelFrame(self.scrollable_frame, text="📊 Leituras Sensores")
        self.frame_sensor.pack(padx=10, pady=5, fill="x")
        self.sensor_labels = {}
        for i, name in enumerate(["Humidity", "Soil", "Temperature"]):
            ttk.Label(self.frame_sensor, text=f"{name}:").grid(row=i, column=0, sticky="e", padx=5, pady=3)
            lbl = ttk.Label(self.frame_sensor, text="---", width=10)
            lbl.grid(row=i, column=1, sticky="w", padx=5, pady=3)
            self.sensor_labels[name] = lbl

        # ==============================
        # CONFIGURAÇÕES HUMID
        # ==============================
        self.frame_humid = ttk.LabelFrame(self.scrollable_frame, text="💧 Configurações Umidade")
        self.frame_humid.pack(padx=10, pady=5, fill="x")
        vcmd_float = (self.root.register(lambda P: P.replace(".", "").replace("-", "").isdigit() or P == "" or P == "-"), "%P")
        
        ttk.Label(self.frame_humid, text="Target Humidity:").grid(row=0, column=0, sticky="e", padx=5, pady=3)
        self.target_humid = ttk.Entry(self.frame_humid, width=8, validate="key", validatecommand=vcmd_float)
        self.target_humid.grid(row=0, column=1, sticky="w", padx=5, pady=3)

        ttk.Label(self.frame_humid, text="Humidity Tolerance:").grid(row=1, column=0, sticky="e", padx=5, pady=3)
        self.humid_tolerance = ttk.Entry(self.frame_humid, width=8, validate="key", validatecommand=vcmd_float)
        self.humid_tolerance.grid(row=1, column=1, sticky="w", padx=5, pady=3)

        btn_frame_humid = tk.Frame(self.frame_humid, bg=colors['bg'])
        btn_frame_humid.grid(row=2, column=0, columnspan=2, pady=10)
        RoundedButton(btn_frame_humid, text="💾 Gravar", command=self.save_humid_config, width=120).pack()

        # ==============================
        # CONFIGURAÇÕES SOIL
        # ==============================
        self.frame_soil = ttk.LabelFrame(self.scrollable_frame, text="🌱 Configurações Solo")
        self.frame_soil.pack(padx=10, pady=5, fill="x")

        ttk.Label(self.frame_soil, text="Target Soil:").grid(row=0, column=0, sticky="e", padx=5, pady=3)
        self.target_soil = ttk.Entry(self.frame_soil, width=8, validate="key", validatecommand=vcmd_float)
        self.target_soil.grid(row=0, column=1, sticky="w", padx=5, pady=3)

        ttk.Label(self.frame_soil, text="Soil Tolerance:").grid(row=1, column=0, sticky="e", padx=5, pady=3)
        self.soil_tolerance = ttk.Entry(self.frame_soil, width=8, validate="key", validatecommand=vcmd_float)
        self.soil_tolerance.grid(row=1, column=1, sticky="w", padx=5, pady=3)

        ttk.Label(self.frame_soil, text="Pump Duration:").grid(row=2, column=0, sticky="e", padx=5, pady=3)
        self.pump_duration = ttk.Entry(self.frame_soil, width=8, validate="key", validatecommand=vcmd_float)
        self.pump_duration.grid(row=2, column=1, sticky="w", padx=5, pady=3)

        ttk.Label(self.frame_soil, text="Absorption Delay:").grid(row=3, column=0, sticky="e", padx=5, pady=3)
        self.absorption_delay = ttk.Entry(self.frame_soil, width=8, validate="key", validatecommand=vcmd_float)
        self.absorption_delay.grid(row=3, column=1, sticky="w", padx=5, pady=3)

        btn_frame_soil = tk.Frame(self.frame_soil, bg=colors['bg'])
        btn_frame_soil.grid(row=4, column=0, columnspan=2, pady=10)
        RoundedButton(btn_frame_soil, text="💾 Gravar", command=self.save_soil_config, width=120).pack()

        # ==============================
        # CONFIGURAÇÕES TEMPERATURE
        # ==============================
        self.frame_temp = ttk.LabelFrame(self.scrollable_frame, text="🌡️ Configurações Temperatura")
        self.frame_temp.pack(padx=10, pady=5, fill="x")

        ttk.Label(self.frame_temp, text="Target Temperature:").grid(row=0, column=0, sticky="e", padx=5, pady=3)
        self.target_temp = ttk.Entry(self.frame_temp, width=8, validate="key", validatecommand=vcmd_float)
        self.target_temp.grid(row=0, column=1, sticky="w", padx=5, pady=3)

        ttk.Label(self.frame_temp, text="Temp Tolerance:").grid(row=1, column=0, sticky="e", padx=5, pady=3)
        self.temp_tolerance = ttk.Entry(self.frame_temp, width=8, validate="key", validatecommand=vcmd_float)
        self.temp_tolerance.grid(row=1, column=1, sticky="w", padx=5, pady=3)

        btn_frame_temp = tk.Frame(self.frame_temp, bg=colors['bg'])
        btn_frame_temp.grid(row=2, column=0, columnspan=2, pady=10)
        RoundedButton(btn_frame_temp, text="💾 Gravar", command=self.save_temp_config, width=120).pack()

        # ==============================
        # HORÁRIOS DA LUZ
        # ==============================
        self.frame_light = ttk.LabelFrame(self.scrollable_frame, text="🕒 Horário da Luz")
        self.frame_light.pack(padx=10, pady=5, fill="x")
        vcmd_int = (self.root.register(lambda P: P.isdigit() or P == ""), "%P")

        ttk.Label(self.frame_light, text="Hora ON:").grid(row=0, column=0, padx=5, pady=5, sticky="e")
        self.hour_on_hh = ttk.Entry(self.frame_light, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_on_mm = ttk.Entry(self.frame_light, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_on_hh.grid(row=0, column=1)
        ttk.Label(self.frame_light, text=":").grid(row=0, column=2)
        self.hour_on_mm.grid(row=0, column=3)

        ttk.Label(self.frame_light, text="Hora OFF:").grid(row=1, column=0, padx=5, pady=5, sticky="e")
        self.hour_off_hh = ttk.Entry(self.frame_light, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_off_mm = ttk.Entry(self.frame_light, width=3, justify="center", validate="key", validatecommand=vcmd_int)
        self.hour_off_hh.grid(row=1, column=1)
        ttk.Label(self.frame_light, text=":").grid(row=1, column=2)
        self.hour_off_mm.grid(row=1, column=3)

        btn_frame_light = tk.Frame(self.frame_light, bg=colors['bg'])
        btn_frame_light.grid(row=2, column=0, columnspan=4, pady=10)
        RoundedButton(btn_frame_light, text="💾 Gravar Horário", command=self.save_times, width=150).pack()

        self.status_label = ttk.Label(self.scrollable_frame, text="", foreground="gray")
        self.status_label.pack(pady=5)

    # ==============================
    # ABA DE GRÁFICOS
    # ==============================
    def create_graphs_tab(self):
        colors = theme_manager.get_colors()
        graphs_frame = ttk.Frame(self.notebook)
        self.notebook.add(graphs_frame, text="📊 Gráficos")

        # Frame de controle
        control_frame = tk.Frame(graphs_frame, bg=colors['bg'])
        control_frame.pack(pady=10, fill='x')

        ttk.Label(control_frame, text="Intervalo de atualização (segundos):").pack(side='left', padx=10)
        
        self.interval_var = tk.StringVar(value="10")
        interval_spinbox = ttk.Spinbox(control_frame, from_=1, to=60, textvariable=self.interval_var, 
                                       width=5, command=self.update_interval)
        interval_spinbox.pack(side='left', padx=5)

        # Botões de gráficos
        buttons_frame = tk.Frame(graphs_frame, bg=colors['bg'])
        buttons_frame.pack(pady=10)

        RoundedButton(buttons_frame, text="💧 Umidade", 
                     command=lambda: self.show_graph("Humidity"), width=150).pack(side='left', padx=5)
        RoundedButton(buttons_frame, text="🌱 Solo", 
                     command=lambda: self.show_graph("Soil"), width=150).pack(side='left', padx=5)
        RoundedButton(buttons_frame, text="🌡️ Temperatura", 
                     command=lambda: self.show_graph("Temperature"), width=150).pack(side='left', padx=5)
        RoundedButton(buttons_frame, text="📊 Geral", 
                     command=lambda: self.show_graph("Geral"), width=150).pack(side='left', padx=5)

        # Frame para exibir o gráfico
        self.graph_display_frame = tk.Frame(graphs_frame, bg=colors['bg'])
        self.graph_display_frame.pack(fill='both', expand=True, padx=10, pady=10)

    def update_interval(self):
        try:
            self.graph_update_interval = int(self.interval_var.get())
        except:
            self.graph_update_interval = 10

    def on_tab_changed(self, event):
        selected_tab = event.widget.tab('current')['text']
        if selected_tab == "📊 Gráficos" and not self.graphs_initialized:
            self.graphs_initialized = True
            self.update_graphs_loop()

    def show_graph(self, graph_type):
        self.current_graph_type = graph_type
        self.generate_and_display_graph()

    def generate_and_display_graph(self):
        # Limpar frame anterior
        for widget in self.graph_display_frame.winfo_children():
            widget.destroy()
        
        if self.current_canvas:
            self.current_canvas = None
        if self.current_toolbar:
            self.current_toolbar = None

        if not os.path.isfile(self.csv_file):
            ttk.Label(self.graph_display_frame, text="⏳ Aguardando coleta de dados...", 
                     font=("Arial", 14)).pack(pady=50)
            return

        try:
            # Ler CSV
            timestamps = []
            humidity = []
            soil = []
            temperature = []
            actuators_data = {
                'cooler': [], 'dehumid': [], 'heater': [], 
                'humid': [], 'light': [], 'pump': []
            }

            with open(self.csv_file, 'r') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    timestamps.append(datetime.strptime(row['timestamp'], '%Y-%m-%d %H:%M:%S'))
                    humidity.append(float(row['humidity']))
                    soil.append(float(row['soil']))
                    temperature.append(float(row['temperature']))
                    for key in actuators_data:
                        actuators_data[key].append(int(row[key]))

            if len(timestamps) == 0:
                ttk.Label(self.graph_display_frame, text="📭 Sem dados suficientes", 
                         font=("Arial", 14)).pack(pady=50)
                return

            # Obter targets e tolerâncias
            humid_data = self.get_ref("InsertedData/Sensor/Humid").get()
            soil_data = self.get_ref("InsertedData/Sensor/Soil").get()
            temp_data = self.get_ref("InsertedData/Sensor/Temperature").get()

            target_humid = float(humid_data.get("TargetHumid", 50)) if humid_data else 50
            humid_tol = float(humid_data.get("HumidTolerance", 5)) if humid_data else 5
            target_soil = float(soil_data.get("TargetSoil", 50)) if soil_data else 50
            soil_tol = float(soil_data.get("SoilTolerance", 5)) if soil_data else 5
            target_temp = float(temp_data.get("TargetTemp", 25)) if temp_data else 25
            temp_tol = float(temp_data.get("TempTolerance", 3)) if temp_data else 3

            # Criar figura
            colors = theme_manager.get_colors()
            fig = Figure(figsize=(12, 7), facecolor=colors['plot_bg'])
            
            if self.current_graph_type == "Humidity":
                self.plot_sensor_graph(fig, timestamps, humidity, target_humid, humid_tol, 
                                      "Umidade (%)", "💧 Histórico de Umidade")
            elif self.current_graph_type == "Soil":
                self.plot_sensor_graph(fig, timestamps, soil, target_soil, soil_tol, 
                                      "Umidade do Solo (%)", "🌱 Histórico do Solo")
            elif self.current_graph_type == "Temperature":
                self.plot_sensor_graph(fig, timestamps, temperature, target_temp, temp_tol, 
                                      "Temperatura (°C)", "🌡️ Histórico de Temperatura")
            elif self.current_graph_type == "Geral":
                self.plot_general_graph(fig, timestamps, humidity, soil, temperature, actuators_data,
                                       target_humid, humid_tol, target_soil, soil_tol, target_temp, temp_tol)
            
            # Exibir no tkinter
            self.current_canvas = FigureCanvasTkAgg(fig, self.graph_display_frame)
            self.current_canvas.draw()
            self.current_canvas.get_tk_widget().pack(fill='both', expand=True)
            
            # Adicionar toolbar de navegação
            self.current_toolbar = NavigationToolbar2Tk(self.current_canvas, self.graph_display_frame)
            self.current_toolbar.update()

        except Exception as e:
            ttk.Label(self.graph_display_frame, text=f"❌ Erro: {str(e)}", 
                     font=("Arial", 12), foreground="red").pack(pady=50)

    def plot_sensor_graph(self, fig, timestamps, values, target, tolerance, ylabel, title):
        colors = theme_manager.get_colors()
        ax = fig.add_subplot(111)
        
        # Configurar fundo
        ax.set_facecolor(colors['plot_bg'])
        
        # Plot principal
        ax.plot(timestamps, values, label='Leitura', color='#2196F3', linewidth=2.5, marker='o', 
                markersize=3, markevery=max(1, len(timestamps)//50))
        
        # Linha target
        ax.axhline(y=target, color='#4CAF50', linestyle=':', linewidth=2, label=f'Target ({target})', alpha=0.8)
        
        # Limites
        upper = target + tolerance
        lower = target - tolerance
        ax.axhline(y=upper, color='#FF5722', linestyle='--', linewidth=1.5, 
                  label=f'Limite Superior ({upper})', alpha=0.7)
        ax.axhline(y=lower, color='#FF5722', linestyle='--', linewidth=1.5, 
                  label=f'Limite Inferior ({lower})', alpha=0.7)
        
        # Área entre limites
        ax.fill_between(timestamps, lower, upper, color='#4CAF50', alpha=0.1)
        
        # Configurações
        ax.set_xlabel('Tempo', fontsize=11, color=colors['plot_fg'])
        ax.set_ylabel(ylabel, fontsize=11, color=colors['plot_fg'])
        ax.set_title(title, fontsize=14, fontweight='bold', color=colors['plot_fg'], pad=20)
        ax.legend(loc='upper left', framealpha=0.9)
        ax.grid(True, alpha=0.3, linestyle='--')
        ax.tick_params(colors=colors['plot_fg'])
        
        # Formato de data
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        fig.autofmt_xdate()
        
        fig.tight_layout()

    def plot_general_graph(self, fig, timestamps, humidity, soil, temperature, actuators_data,
                          target_humid, humid_tol, target_soil, soil_tol, target_temp, temp_tol):
        colors = theme_manager.get_colors()
        
        # Criar 2 subplots
        ax1 = fig.add_subplot(211)
        ax2 = fig.add_subplot(212)
        
        ax1.set_facecolor(colors['plot_bg'])
        ax2.set_facecolor(colors['plot_bg'])
        
        # ===== SUBPLOT 1: Sensores =====
        ax1.plot(timestamps, humidity, label='Umidade', color='#2196F3', linewidth=2, marker='o', 
                markersize=2, markevery=max(1, len(timestamps)//50))
        ax1.plot(timestamps, soil, label='Solo', color='#8B4513', linewidth=2, marker='s', 
                markersize=2, markevery=max(1, len(timestamps)//50))
        ax1.plot(timestamps, temperature, label='Temperatura', color='#FF5722', linewidth=2, marker='^', 
                markersize=2, markevery=max(1, len(timestamps)//50))
        
        # Linhas de target (discretas)
        ax1.axhline(y=target_humid, color='#2196F3', linestyle=':', linewidth=1, alpha=0.4)
        ax1.axhline(y=target_soil, color='#8B4513', linestyle=':', linewidth=1, alpha=0.4)
        ax1.axhline(y=target_temp, color='#FF5722', linestyle=':', linewidth=1, alpha=0.4)
        
        ax1.set_ylabel('Valores', fontsize=11, color=colors['plot_fg'])
        ax1.set_title('📊 Dashboard Completo - Sensores', fontsize=14, fontweight='bold', 
                     color=colors['plot_fg'], pad=15)
        ax1.legend(loc='upper left', framealpha=0.9, ncol=3)
        ax1.grid(True, alpha=0.3, linestyle='--')
        ax1.tick_params(colors=colors['plot_fg'])
        
        # ===== SUBPLOT 2: Atuadores =====
        actuator_info = {
            'cooler': {'label': 'Cooler', 'color': '#00BCD4', 'y': 5},
            'dehumid': {'label': 'Dehumid', 'color': '#FF9800', 'y': 4},
            'heater': {'label': 'Heater', 'color': '#F44336', 'y': 3},
            'humid': {'label': 'Humid', 'color': '#4CAF50', 'y': 2},
            'light': {'label': 'Light', 'color': '#FFEB3B', 'y': 1},
            'pump': {'label': 'Pump', 'color': '#9C27B0', 'y': 0}
        }
        
        for actuator, data in actuators_data.items():
            info = actuator_info[actuator]
            for i, (t, state) in enumerate(zip(timestamps, data)):
                if state == 1:
                    ax2.scatter(t, info['y'], marker='s', s=80, color=info['color'], 
                              alpha=0.8, edgecolors='white', linewidths=0.5)
        
        # Configurar eixo Y com nomes dos atuadores
        ax2.set_yticks([info['y'] for info in actuator_info.values()])
        ax2.set_yticklabels([info['label'] for info in actuator_info.values()])
        ax2.set_xlabel('Tempo', fontsize=11, color=colors['plot_fg'])
        ax2.set_ylabel('Atuadores', fontsize=11, color=colors['plot_fg'])
        ax2.set_title('⚙️ Status dos Atuadores', fontsize=12, fontweight='bold', 
                     color=colors['plot_fg'], pad=10)
        ax2.grid(True, alpha=0.3, linestyle='--', axis='x')
        ax2.tick_params(colors=colors['plot_fg'])
        
        # Formato de data para ambos
        ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        ax2.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M'))
        fig.autofmt_xdate()
        
        fig.tight_layout()

    def update_graphs_loop(self):
        if self.graphs_initialized and self.current_graph_type:
            try:
                self.generate_and_display_graph()
            except Exception as e:
                print(f"Erro ao atualizar gráfico: {e}")
        
        if self.graphs_initialized:
            self.root.after(self.graph_update_interval * 1000, self.update_graphs_loop)

    # ==============================
    # FUNÇÕES AUXILIARES
    # ==============================
    def get_ref(self, path): 
        return db.reference(self.user_path + path)

    def get_global_ref(self, path):
        return db.reference(path)

    def load_initial_data(self):
        self.load_global_config()
        self.load_humid_config()
        self.load_soil_config()
        self.load_temp_config()
        self.load_times()

    def load_global_config(self):
        try:
            binary = self.get_global_ref("_Binary").get()
            token = self.get_global_ref("_Token").get()
            has_update = self.get_global_ref("_HasUpdate").get()
            need_change = self.get_ref("needChange").get()

            if binary:
                self.binary_current.config(text=f"Atual: {binary[:50]}...")
            if token:
                self.token_current.config(text=f"Atual: {token[:30]}...")
            
            if has_update is not None:
                self.has_update_var.set(has_update)
                status_text = "✓ True" if has_update else "✗ False"
                color = "green" if has_update else "red"
                self.has_update_status.config(text=f"(Servidor: {status_text})", foreground=color)
            
            if need_change is not None:
                self.need_change_var.set(need_change)
                status_text = "✓ True" if need_change else "✗ False"
                color = "green" if need_change else "red"
                self.need_change_status.config(text=f"(Servidor: {status_text})", foreground=color)
        except Exception as e:
            print(f"Erro ao carregar config global: {e}")

    def load_humid_config(self):
        data = self.get_ref("InsertedData/Sensor/Humid").get()
        if data:
            self.target_humid.delete(0, tk.END)
            self.target_humid.insert(0, str(data.get("TargetHumid", "")))
            self.humid_tolerance.delete(0, tk.END)
            self.humid_tolerance.insert(0, str(data.get("HumidTolerance", "")))

    def load_soil_config(self):
        data = self.get_ref("InsertedData/Sensor/Soil").get()
        if data:
            self.target_soil.delete(0, tk.END)
            self.target_soil.insert(0, str(data.get("TargetSoil", "")))
            self.soil_tolerance.delete(0, tk.END)
            self.soil_tolerance.insert(0, str(data.get("SoilTolerance", "")))
            self.pump_duration.delete(0, tk.END)
            self.pump_duration.insert(0, str(data.get("PumpDuration", "")))
            self.absorption_delay.delete(0, tk.END)
            self.absorption_delay.insert(0, str(data.get("AbsorptionDelay", "")))

    def load_temp_config(self):
        data = self.get_ref("InsertedData/Sensor/Temperature").get()
        if data:
            self.target_temp.delete(0, tk.END)
            self.target_temp.insert(0, str(data.get("TargetTemp", "")))
            self.temp_tolerance.delete(0, tk.END)
            self.temp_tolerance.insert(0, str(data.get("TempTolerance", "")))

    def load_times(self):
        data = self.get_ref("InsertedData/Light").get()
        if data:
            hour_on = data.get("HourOn", "00:00")
            hour_off = data.get("HourOff", "00:00")
            
            hh_on, mm_on = hour_on.split(":")
            self.hour_on_hh.delete(0, tk.END)
            self.hour_on_hh.insert(0, hh_on)
            self.hour_on_mm.delete(0, tk.END)
            self.hour_on_mm.insert(0, mm_on)

            hh_off, mm_off = hour_off.split(":")
            self.hour_off_hh.delete(0, tk.END)
            self.hour_off_hh.insert(0, hh_off)
            self.hour_off_mm.delete(0, tk.END)
            self.hour_off_mm.insert(0, mm_off)

    # ==============================
    # ATUALIZAÇÃO AUTOMÁTICA
    # ==============================
    def update_data(self):
        threading.Thread(target=self._refresh, daemon=True).start()
        self.root.after(5000, self.update_data)

    def _refresh(self):
        try:
            actuators = self.get_ref("Readings/Actuator").get()
            sensors = self.get_ref("Readings/Sensor").get()
            
            if actuators:
                for key in ["Cooler", "Dehumid", "Heater", "Humid", "Light", "Pump"]:
                    status_key = f"{key}Status"
                    if status_key in actuators:
                        state = actuators[status_key]
                        self.actuator_labels[key].config(
                            text="ON" if state else "OFF", 
                            foreground="green" if state else "red"
                        )
            
            if sensors:
                for key in ["Humidity", "Soil", "Temperature"]:
                    val = sensors.get(key, "---")
                    self.sensor_labels[key].config(text=str(val))

                self.save_to_csv(sensors, actuators)

            self.load_global_config()

        except Exception as e:
            self.status_label.config(text=f"Erro: {e}")

    def save_to_csv(self, sensors, actuators):
        try:
            file_exists = os.path.isfile(self.csv_file)
            
            with open(self.csv_file, 'a', newline='') as f:
                fieldnames = ['timestamp', 'humidity', 'soil', 'temperature', 
                             'cooler', 'dehumid', 'heater', 'humid', 'light', 'pump']
                writer = csv.DictWriter(f, fieldnames=fieldnames)
                
                if not file_exists:
                    writer.writeheader()
                
                writer.writerow({
                    'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    'humidity': sensors.get('Humidity', 0),
                    'soil': sensors.get('Soil', 0),
                    'temperature': sensors.get('Temperature', 0),
                    'cooler': 1 if actuators.get('CoolerStatus') else 0,
                    'dehumid': 1 if actuators.get('DehumidStatus') else 0,
                    'heater': 1 if actuators.get('HeaterStatus') else 0,
                    'humid': 1 if actuators.get('HumidStatus') else 0,
                    'light': 1 if actuators.get('LightStatus') else 0,
                    'pump': 1 if actuators.get('PumpStatus') else 0
                })
        except Exception as e:
            print(f"Erro ao salvar CSV: {e}")

    # ==============================
    # SALVAR CONFIGURAÇÕES
    # ==============================
    def save_global_config(self):
        try:
            binary = self.binary_entry.get().strip()
            token = self.token_entry.get().strip()
            
            if binary:
                self.get_global_ref("_Binary").set(binary)
            if token:
                self.get_global_ref("_Token").set(token)
            
            self.get_global_ref("_HasUpdate").set(self.has_update_var.get())
            self.get_ref("needChange").set(self.need_change_var.get())
            
            messagebox.showinfo("Sucesso", "Configurações globais gravadas!")
            self.load_global_config()
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_humid_config(self):
        try:
            data = {
                "TargetHumid": float(self.target_humid.get()),
                "HumidTolerance": float(self.humid_tolerance.get())
            }
            self.get_ref("InsertedData/Sensor/Humid").update(data)
            messagebox.showinfo("Sucesso", "Config de umidade gravada!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_soil_config(self):
        try:
            data = {
                "TargetSoil": float(self.target_soil.get()),
                "SoilTolerance": float(self.soil_tolerance.get()),
                "PumpDuration": float(self.pump_duration.get()),
                "AbsorptionDelay": float(self.absorption_delay.get())
            }
            self.get_ref("InsertedData/Sensor/Soil").update(data)
            messagebox.showinfo("Sucesso", "Config de solo gravada!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_temp_config(self):
        try:
            data = {
                "TargetTemp": float(self.target_temp.get()),
                "TempTolerance": float(self.temp_tolerance.get())
            }
            self.get_ref("InsertedData/Sensor/Temperature").update(data)
            messagebox.showinfo("Sucesso", "Config de temperatura gravada!")
        except Exception as e:
            messagebox.showerror("Erro", str(e))

    def save_times(self):
        try:
            hh_on, mm_on = self.hour_on_hh.get(), self.hour_on_mm.get()
            hh_off, mm_off = self.hour_off_hh.get(), self.hour_off_mm.get()
            
            if not all(x.isdigit() for x in [hh_on, mm_on, hh_off, mm_off]):
                return messagebox.showerror("Erro", "Somente números permitidos.")
            
            if not (0 <= int(hh_on) <= 23 and 0 <= int(mm_on) <= 59 and 
                    0 <= int(hh_off) <= 23 and 0 <= int(mm_off) <= 59):
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