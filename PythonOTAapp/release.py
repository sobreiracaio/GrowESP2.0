#!/usr/bin/env python3
"""
ESP32 GitHub Release Manager
Interface gráfica para criar releases automaticamente
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import requests
import json
import subprocess
import threading
from pathlib import Path
from datetime import datetime
import os


class ESP32ReleaseManager:
    def __init__(self, root):
        self.root = root
        self.root.title("🚀 ESP32 Release Manager")
        self.root.geometry("1150x800")
        self.root.minsize(950, 500)
        self.root.resizable(True, True)

        self.config_file = "release_config.json"
        self.load_config()
        self.create_widgets()

    # ─────────────────────────────────────────────
    # CONFIG
    # ─────────────────────────────────────────────
    def load_config(self):
        if Path(self.config_file).exists():
            with open(self.config_file, 'r') as f:
                config = json.load(f)
                self.saved_token        = config.get('token', '')
                self.saved_owner        = config.get('owner', '')
                self.saved_repo         = config.get('repo', '')
                self.saved_env          = config.get('environment', 'esp32')
                self.saved_project_dir  = config.get('project_dir', '')
                self.saved_fb_email     = config.get('fb_email', '')
                self.saved_fb_password  = config.get('fb_password', '')
                self.saved_fb_apikey    = config.get('fb_apikey', '')
                self.saved_fb_db_url    = config.get('fb_db_url', '')
        else:
            self.saved_token = self.saved_owner = self.saved_repo = ''
            self.saved_env = 'esp32'
            self.saved_project_dir = ''
            self.saved_fb_email = self.saved_fb_password = ''
            self.saved_fb_apikey = self.saved_fb_db_url = ''

    def save_config(self):
        config = {
            'token':        self.token_entry.get(),
            'owner':        self.owner_entry.get(),
            'repo':         self.repo_entry.get(),
            'environment':  self.env_entry.get(),
            'project_dir':  self.project_dir.get(),
            'fb_email':     self.fb_email_entry.get(),
            'fb_password':  self.fb_password_entry.get(),
            'fb_apikey':    self.fb_apikey_entry.get(),
            'fb_db_url':    self.fb_dburl_entry.get(),
        }
        with open(self.config_file, 'w') as f:
            json.dump(config, f, indent=2)
        messagebox.showinfo("Sucesso", "Configurações salvas!")
        self.log("Configurações salvas com sucesso", "SUCCESS")

    # ─────────────────────────────────────────────
    # UI
    # ─────────────────────────────────────────────
    def create_widgets(self):
        main = ttk.Frame(self.root)
        main.pack(fill="both", expand=True, padx=8, pady=8)
        main.columnconfigure(0, weight=1)
        main.columnconfigure(1, weight=1)
        main.rowconfigure(0, weight=1)

        # ══════════════════════════════════════════
        # COLUNA ESQUERDA — Canvas + Scrollbar
        # ══════════════════════════════════════════
        left_outer = ttk.Frame(main)
        left_outer.grid(row=0, column=0, sticky="nsew", padx=(0, 4))
        left_outer.columnconfigure(0, weight=1)
        left_outer.rowconfigure(0, weight=1)

        left_canvas = tk.Canvas(left_outer, borderwidth=0, highlightthickness=0)
        left_canvas.grid(row=0, column=0, sticky="nsew")

        left_vsb = ttk.Scrollbar(left_outer, orient="vertical",
                                  command=left_canvas.yview)
        left_vsb.grid(row=0, column=1, sticky="ns")
        left_canvas.configure(yscrollcommand=left_vsb.set)

        # Frame interno que vai dentro do canvas
        left = ttk.Frame(left_canvas)
        left.columnconfigure(0, weight=1)

        left_window = left_canvas.create_window((0, 0), window=left, anchor="nw")

        # Ajusta o canvas quando o frame interno muda de tamanho
        def _on_left_configure(event):
            left_canvas.configure(scrollregion=left_canvas.bbox("all"))

        def _on_canvas_resize(event):
            left_canvas.itemconfig(left_window, width=event.width)

        left.bind("<Configure>", _on_left_configure)
        left_canvas.bind("<Configure>", _on_canvas_resize)

        # Scroll com roda do mouse na coluna esquerda
        def _on_mousewheel(event):
            left_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        def _on_mousewheel_linux(event):
            if event.num == 4:
                left_canvas.yview_scroll(-1, "units")
            elif event.num == 5:
                left_canvas.yview_scroll(1, "units")

        left_canvas.bind("<MouseWheel>", _on_mousewheel)          # Windows/Mac
        left_canvas.bind("<Button-4>", _on_mousewheel_linux)      # Linux scroll up
        left_canvas.bind("<Button-5>", _on_mousewheel_linux)      # Linux scroll down
        left.bind("<MouseWheel>", _on_mousewheel)
        left.bind("<Button-4>", _on_mousewheel_linux)
        left.bind("<Button-5>", _on_mousewheel_linux)

        # ── GitHub config ──
        cfg = ttk.LabelFrame(left, text="⚙️  Configurações do GitHub", padding=8)
        cfg.grid(row=0, column=0, sticky="ew", pady=(0, 4))
        cfg.columnconfigure(1, weight=1)

        ttk.Label(cfg, text="Token do GitHub:").grid(row=0, column=0, sticky="w", pady=2)
        self.token_entry = ttk.Entry(cfg, show="*")
        self.token_entry.grid(row=0, column=1, sticky="ew", pady=2, padx=5)
        self.token_entry.insert(0, self.saved_token)

        ttk.Label(cfg, text="Owner (usuário):").grid(row=1, column=0, sticky="w", pady=2)
        self.owner_entry = ttk.Entry(cfg)
        self.owner_entry.grid(row=1, column=1, sticky="ew", pady=2, padx=5)
        self.owner_entry.insert(0, self.saved_owner)

        ttk.Label(cfg, text="Repositório:").grid(row=2, column=0, sticky="w", pady=2)
        self.repo_entry = ttk.Entry(cfg)
        self.repo_entry.grid(row=2, column=1, sticky="ew", pady=2, padx=5)
        self.repo_entry.insert(0, self.saved_repo)

        ttk.Label(cfg, text="PlatformIO Env:").grid(row=3, column=0, sticky="w", pady=2)
        self.env_entry = ttk.Entry(cfg)
        self.env_entry.grid(row=3, column=1, sticky="ew", pady=2, padx=5)
        self.env_entry.insert(0, self.saved_env)

        ttk.Label(cfg, text="Pasta do Projeto:").grid(row=4, column=0, sticky="w", pady=2)
        pdir_frame = ttk.Frame(cfg)
        pdir_frame.grid(row=4, column=1, sticky="ew", pady=2, padx=5)
        pdir_frame.columnconfigure(0, weight=1)
        self.project_dir = tk.StringVar(value=self.saved_project_dir)
        ttk.Entry(pdir_frame, textvariable=self.project_dir,
                  state="readonly").grid(row=0, column=0, sticky="ew")
        ttk.Button(pdir_frame, text="📁", width=3,
                   command=self.select_project_dir).grid(row=0, column=1, padx=(4, 0))

        btn_cfg = ttk.Frame(cfg)
        btn_cfg.grid(row=5, column=0, columnspan=2, pady=8)
        ttk.Button(btn_cfg, text="💾 Salvar Configurações",
                   command=self.save_config).pack(side="left", padx=4)
        ttk.Button(btn_cfg, text="🔍 Testar Conexão",
                   command=self.test_connection).pack(side="left", padx=4)

        # ── Firebase config ──
        fb = ttk.LabelFrame(left, text="🔥 Firebase Realtime Database", padding=8)
        fb.grid(row=1, column=0, sticky="ew", pady=(0, 4))
        fb.columnconfigure(1, weight=1)

        ttk.Label(fb, text="E-mail:").grid(row=0, column=0, sticky="w", pady=2)
        self.fb_email_entry = ttk.Entry(fb)
        self.fb_email_entry.grid(row=0, column=1, sticky="ew", pady=2, padx=5)
        self.fb_email_entry.insert(0, self.saved_fb_email)

        ttk.Label(fb, text="Senha:").grid(row=1, column=0, sticky="w", pady=2)
        self.fb_password_entry = ttk.Entry(fb, show="*")
        self.fb_password_entry.grid(row=1, column=1, sticky="ew", pady=2, padx=5)
        self.fb_password_entry.insert(0, self.saved_fb_password)

        ttk.Label(fb, text="API Key:").grid(row=2, column=0, sticky="w", pady=2)
        self.fb_apikey_entry = ttk.Entry(fb)
        self.fb_apikey_entry.grid(row=2, column=1, sticky="ew", pady=2, padx=5)
        self.fb_apikey_entry.insert(0, self.saved_fb_apikey)

        ttk.Label(fb, text="Database URL:").grid(row=3, column=0, sticky="w", pady=2)
        self.fb_dburl_entry = ttk.Entry(fb)
        self.fb_dburl_entry.grid(row=3, column=1, sticky="ew", pady=2, padx=5)
        self.fb_dburl_entry.insert(0, self.saved_fb_db_url)

        ttk.Label(fb, text="_Binary URL:").grid(row=4, column=0, sticky="w", pady=2)
        binary_frame = ttk.Frame(fb)
        binary_frame.grid(row=4, column=1, sticky="ew", pady=2, padx=5)
        binary_frame.columnconfigure(0, weight=1)
        self.fb_binary_var = tk.StringVar()
        ttk.Entry(binary_frame, textvariable=self.fb_binary_var).grid(
            row=0, column=0, sticky="ew")
        ttk.Button(binary_frame, text="📥", width=3,
                   command=self.firebase_load_binary).grid(row=0, column=1, padx=(4, 0))
        ttk.Button(binary_frame, text="📤", width=3,
                   command=self.firebase_save_binary).grid(row=0, column=2, padx=(2, 0))

        ttk.Label(fb, text="📥 Carregar do Firebase    📤 Enviar para Firebase",
                  foreground="gray", font=("", 8)).grid(
            row=5, column=0, columnspan=2, sticky="w", padx=5, pady=(0, 2))

        # ── Modo de versionamento ──
        ver = ttk.LabelFrame(left, text="📋 Modo de Versionamento", padding=8)
        ver.grid(row=2, column=0, sticky="ew", pady=(0, 4))

        self.version_mode = tk.StringVar(value="fixed")
        ttk.Radiobutton(ver, text="Tag Fixa (sempre 'update')",
                        variable=self.version_mode, value="fixed",
                        command=self.toggle_version_mode).pack(anchor="w", pady=1)
        ttk.Radiobutton(ver, text="Versão Incremental (v1.0.0, v1.0.1...)",
                        variable=self.version_mode, value="incremental",
                        command=self.toggle_version_mode).pack(anchor="w", pady=1)
        ttk.Radiobutton(ver, text="Versão Manual (você escolhe)",
                        variable=self.version_mode, value="manual",
                        command=self.toggle_version_mode).pack(anchor="w", pady=1)

        ver_inp = ttk.Frame(ver)
        ver_inp.pack(fill="x", pady=4)
        ttk.Label(ver_inp, text="Tag/Versão:").pack(side="left", padx=4)
        self.version_entry = ttk.Entry(ver_inp, width=20)
        self.version_entry.pack(side="left", padx=4)
        self.version_entry.insert(0, "update")
        self.version_entry.config(state="disabled")

        # ── Firmware ──
        firm = ttk.LabelFrame(left, text="📦 Firmware", padding=8)
        firm.grid(row=3, column=0, sticky="ew", pady=(0, 4))

        self.firmware_mode = tk.StringVar(value="build")
        ttk.Radiobutton(firm, text="Compilar com PlatformIO",
                        variable=self.firmware_mode, value="build").pack(anchor="w", pady=1)
        ttk.Radiobutton(firm, text="Selecionar arquivo .bin existente",
                        variable=self.firmware_mode, value="file").pack(anchor="w", pady=1)

        file_row = ttk.Frame(firm)
        file_row.pack(fill="x", pady=4)
        file_row.columnconfigure(0, weight=1)
        self.file_path = tk.StringVar()
        ttk.Entry(file_row, textvariable=self.file_path,
                  state="readonly").pack(side="left", fill="x", expand=True, padx=(0, 4))
        ttk.Button(file_row, text="📂 Selecionar",
                   command=self.select_file).pack(side="left")

        # ── Descrição ──
        desc = ttk.LabelFrame(left, text="📝 Descrição da Release", padding=8)
        desc.grid(row=4, column=0, sticky="ew", pady=(0, 4))

        self.desc_text = scrolledtext.ScrolledText(desc, height=4, wrap=tk.WORD)
        self.desc_text.pack(fill="both", expand=True)
        self.desc_text.insert("1.0", "Atualização automática do firmware\n\nAlterações:\n- ")

        # ── Botão principal ──
        self.create_button = ttk.Button(left, text="🚀  CRIAR RELEASE",
                                        command=self.start_release_process)
        self.create_button.grid(row=5, column=0, sticky="ew", ipady=8, pady=(0, 4))

        self.progress = ttk.Progressbar(left, mode="indeterminate")
        self.progress.grid(row=6, column=0, sticky="ew", pady=(0, 4))

        # ══════════════════════════════════════════
        # COLUNA DIREITA
        # ══════════════════════════════════════════
        right = ttk.Frame(main)
        right.grid(row=0, column=1, sticky="nsew", padx=(4, 0))
        right.columnconfigure(0, weight=1)
        right.rowconfigure(0, weight=2)
        right.rowconfigure(1, weight=3)

        # ── Log ──
        log_frame = ttk.LabelFrame(right, text="📊 Log de Atividades", padding=8)
        log_frame.grid(row=0, column=0, sticky="nsew", pady=(0, 4))
        log_frame.columnconfigure(0, weight=1)
        log_frame.rowconfigure(0, weight=1)

        self.log_text = scrolledtext.ScrolledText(
            log_frame, state="disabled",
            bg="#1e1e1e", fg="#00ff00", font=("Courier", 9))
        self.log_text.grid(row=0, column=0, sticky="nsew")

        ttk.Button(log_frame, text="🗑 Limpar Log",
                   command=self.clear_log).grid(row=1, column=0, sticky="e", pady=(4, 0))

        # ── Lista de releases ──
        rel_frame = ttk.LabelFrame(right, text="📦 Releases do Repositório", padding=8)
        rel_frame.grid(row=1, column=0, sticky="nsew")
        rel_frame.columnconfigure(0, weight=1)
        rel_frame.rowconfigure(1, weight=1)

        rel_btn_row = ttk.Frame(rel_frame)
        rel_btn_row.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 4))
        ttk.Button(rel_btn_row, text="🔄 Atualizar Lista",
                   command=self.load_releases_thread).pack(side="left", padx=(0, 4))
        ttk.Button(rel_btn_row, text="🗑 Apagar Selecionada",
                   command=self.delete_selected_release).pack(side="left")

        cols = ("tag", "nome", "data", "descricao")
        self.releases_tree = ttk.Treeview(rel_frame, columns=cols,
                                          show="headings", height=8)
        self.releases_tree.heading("tag",       text="Tag")
        self.releases_tree.heading("nome",      text="Nome")
        self.releases_tree.heading("data",      text="Criada em")
        self.releases_tree.heading("descricao", text="Descrição")

        self.releases_tree.column("tag",       width=80,  minwidth=80,  stretch=False, anchor="w")
        self.releases_tree.column("nome",      width=120, minwidth=120, stretch=False, anchor="w")
        self.releases_tree.column("data",      width=130, minwidth=130, stretch=False, anchor="center")
        self.releases_tree.column("descricao", width=180, minwidth=100, stretch=True,  anchor="w")

        vsb = ttk.Scrollbar(rel_frame, orient="vertical",
                            command=self.releases_tree.yview)
        self.releases_tree.configure(yscrollcommand=vsb.set)
        self.releases_tree.grid(row=1, column=0, sticky="nsew")
        vsb.grid(row=1, column=1, sticky="ns")

        self.releases_cache = {}

    # ─────────────────────────────────────────────
    # HELPERS
    # ─────────────────────────────────────────────
    def toggle_version_mode(self):
        state = "normal" if self.version_mode.get() == "manual" else "disabled"
        self.version_entry.config(state=state)

    def select_file(self):
        f = filedialog.askopenfilename(
            title="Selecione o firmware",
            filetypes=[("Arquivos BIN", "*.bin"), ("Todos os arquivos", "*.*")])
        if f:
            self.file_path.set(f)

    def select_project_dir(self):
        directory = filedialog.askdirectory(
            title="Selecione a pasta do projeto PlatformIO",
            initialdir=self.project_dir.get() or os.path.expanduser("~"))
        if not directory:
            return
        if (Path(directory) / "platformio.ini").exists():
            self.project_dir.set(directory)
            self.log(f"Pasta do projeto: {directory}", "SUCCESS")
        else:
            if messagebox.askyesno("Aviso",
                    f"Pasta sem 'platformio.ini'!\n\n{directory}\n\nUsar mesmo assim?"):
                self.project_dir.set(directory)

    def log(self, message, level="INFO"):
        icons = {"INFO": "ℹ️", "SUCCESS": "✅", "ERROR": "❌",
                 "WARNING": "⚠️", "BUILD": "🔨", "UPLOAD": "📤", "DELETE": "🗑️"}
        ts  = datetime.now().strftime("%H:%M:%S")
        msg = f"[{ts}] {icons.get(level, '•')} {message}\n"
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, msg)
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")
        self.root.update()

    def clear_log(self):
        self.log_text.config(state="normal")
        self.log_text.delete("1.0", tk.END)
        self.log_text.config(state="disabled")

    def _headers(self):
        return {
            "Authorization": f"token {self.token_entry.get().strip()}",
            "Accept": "application/vnd.github.v3+json"
        }

    def _repo_base(self):
        o = self.owner_entry.get().strip()
        r = self.repo_entry.get().strip()
        return f"https://api.github.com/repos/{o}/{r}"

    # ─────────────────────────────────────────────
    # FIREBASE
    # ─────────────────────────────────────────────
    def _firebase_token(self):
        api_key  = self.fb_apikey_entry.get().strip()
        email    = self.fb_email_entry.get().strip()
        password = self.fb_password_entry.get().strip()
        if not all([api_key, email, password]):
            raise ValueError("Preencha E-mail, Senha e API Key do Firebase!")
        url  = (f"https://identitytoolkit.googleapis.com/v1/"
                f"accounts:signInWithPassword?key={api_key}")
        resp = requests.post(url, json={
            "email": email, "password": password, "returnSecureToken": True
        }, timeout=10)
        if resp.status_code != 200:
            err = resp.json().get("error", {}).get("message", "Erro desconhecido")
            raise ValueError(f"Falha no login Firebase: {err}")
        return resp.json()["idToken"]

    def _firebase_db_url(self):
        url = self.fb_dburl_entry.get().strip().rstrip("/")
        if not url:
            raise ValueError("Database URL do Firebase não preenchida!")
        return url

    def firebase_load_binary(self):
        threading.Thread(target=self._do_firebase_load, daemon=True).start()

    def _do_firebase_load(self):
        try:
            self.log("Conectando ao Firebase...", "INFO")
            token    = self._firebase_token()
            db_url   = self._firebase_db_url()
            endpoint = f"{db_url}/_Binary.json?auth={token}"
            resp = requests.get(endpoint, timeout=10)
            if resp.status_code != 200:
                self.log(f"Erro ao ler Firebase: {resp.status_code} — {resp.text}", "ERROR")
                return
            value = resp.json()
            if value is None:
                self.log("Nó /_Binary está vazio ou não existe.", "WARNING")
                self.root.after(0, lambda: self.fb_binary_var.set(""))
            else:
                val_str = str(value)
                self.log(f"/_Binary carregado: {val_str}", "SUCCESS")
                self.root.after(0, lambda: self.fb_binary_var.set(val_str))
        except ValueError as e:
            self.log(str(e), "ERROR")
            self.root.after(0, lambda: messagebox.showerror("Firebase", str(e)))
        except Exception as e:
            self.log(f"Erro Firebase: {e}", "ERROR")
            self.root.after(0, lambda: messagebox.showerror("Firebase", str(e)))

    def firebase_save_binary(self):
        valor = self.fb_binary_var.get().strip()
        if not valor:
            messagebox.showwarning("Aviso", "O campo _Binary URL está vazio!")
            return
        if not messagebox.askyesno("Confirmar",
                f"Enviar para /_Binary no Firebase?\n\n{valor}"):
            return
        threading.Thread(target=self._do_firebase_save, args=(valor,), daemon=True).start()

    def _do_firebase_save(self, valor):
        try:
            self.log("Enviando /_Binary para o Firebase...", "UPLOAD")
            token    = self._firebase_token()
            db_url   = self._firebase_db_url()
            endpoint = f"{db_url}/_Binary.json?auth={token}"
            resp = requests.put(endpoint, json=valor, timeout=10)
            if resp.status_code == 200:
                self.log(f"/_Binary atualizado: {valor}", "SUCCESS")
                self.root.after(0, lambda: messagebox.showinfo(
                    "Firebase ✅", f"/_Binary atualizado!\n\n{valor}"))
            else:
                self.log(f"Erro ao salvar: {resp.status_code} — {resp.text}", "ERROR")
                self.root.after(0, lambda: messagebox.showerror(
                    "Firebase", f"Erro {resp.status_code}:\n{resp.text}"))
        except ValueError as e:
            self.log(str(e), "ERROR")
            self.root.after(0, lambda: messagebox.showerror("Firebase", str(e)))
        except Exception as e:
            self.log(f"Erro Firebase: {e}", "ERROR")
            self.root.after(0, lambda: messagebox.showerror("Firebase", str(e)))

    # ─────────────────────────────────────────────
    # TESTAR CONEXÃO GITHUB
    # ─────────────────────────────────────────────
    def test_connection(self):
        token = self.token_entry.get().strip()
        owner = self.owner_entry.get().strip()
        repo  = self.repo_entry.get().strip()
        if not token or not owner or not repo:
            messagebox.showerror("Erro", "Preencha todos os campos antes de testar!")
            return
        self.log("Testando conexão com GitHub...", "INFO")
        self.progress.start()
        try:
            r = requests.get("https://api.github.com/user",
                             headers=self._headers(), timeout=10)
            if r.status_code != 200:
                self.log(f"Token inválido! Status: {r.status_code}", "ERROR")
                messagebox.showerror("Erro", f"Token inválido!\nStatus: {r.status_code}")
                return
            user_data = r.json()
            self.log(f"Token válido! Usuário: {user_data.get('login')}", "SUCCESS")
            r2 = requests.get(f"https://api.github.com/repos/{owner}/{repo}",
                              headers=self._headers(), timeout=10)
            if r2.status_code == 200:
                rd = r2.json()
                self.log(f"Repositório: {rd.get('full_name')}", "SUCCESS")
                messagebox.showinfo("Sucesso ✅",
                    f"Conexão OK!\n\nUsuário: {user_data.get('login')}\n"
                    f"Repo: {rd.get('full_name')}\n"
                    f"Privado: {'Sim' if rd.get('private') else 'Não'}")
                self.load_releases_thread()
            elif r2.status_code == 404:
                messagebox.showerror("Erro", f"Repositório '{owner}/{repo}' não encontrado!")
            else:
                messagebox.showerror("Erro", f"Erro ao acessar repositório: {r2.status_code}")
        except requests.exceptions.Timeout:
            messagebox.showerror("Erro", "Timeout! Verifique sua conexão.")
        except Exception as e:
            messagebox.showerror("Erro", str(e))
        finally:
            self.progress.stop()

    # ─────────────────────────────────────────────
    # LISTA DE RELEASES
    # ─────────────────────────────────────────────
    def load_releases_thread(self):
        threading.Thread(target=self.load_releases, daemon=True).start()

    def load_releases(self):
        try:
            self.log("Carregando lista de releases...", "INFO")
            r = requests.get(f"{self._repo_base()}/releases",
                             headers=self._headers(), timeout=10)
            if r.status_code != 200:
                self.log(f"Erro ao carregar releases: {r.status_code}", "ERROR")
                return
            releases = r.json()
            self.releases_cache.clear()
            self.root.after(0, lambda: self.releases_tree.delete(
                *self.releases_tree.get_children()))
            for rel in releases:
                tag    = rel.get("tag_name", "")
                name   = rel.get("name", "")
                raw_dt = rel.get("created_at", "")
                body   = (rel.get("body") or "").replace("\r\n", " ").replace("\n", " ").strip()
                resumo = body[:80] + ("…" if len(body) > 80 else "")
                try:
                    dt = datetime.strptime(raw_dt, "%Y-%m-%dT%H:%M:%SZ").strftime("%d/%m/%Y %H:%M")
                except Exception:
                    dt = raw_dt
                self.releases_cache[tag] = rel

                def _insert(tag=tag, name=name, dt=dt, resumo=resumo):
                    self.releases_tree.insert("", "end", iid=tag,
                                             values=(tag, name, dt, resumo))
                self.root.after(0, _insert)
            self.log(f"{len(releases)} release(s) carregada(s).", "SUCCESS")
        except Exception as e:
            self.log(f"Erro ao carregar releases: {e}", "ERROR")

    def delete_selected_release(self):
        sel = self.releases_tree.selection()
        if not sel:
            messagebox.showwarning("Aviso", "Selecione uma release para apagar!")
            return
        tag = sel[0]
        if not messagebox.askyesno("Confirmar exclusão",
                f"Deseja apagar a release e a tag '{tag}'?\n\nEssa ação não pode ser desfeita."):
            return
        threading.Thread(target=self._do_delete_release, args=(tag,), daemon=True).start()

    def _do_delete_release(self, tag):
        success = self.delete_release(tag)
        if success:
            self.root.after(0, lambda: self.releases_tree.delete(tag))
            self.releases_cache.pop(tag, None)
            self.log(f"Release '{tag}' removida da lista.", "SUCCESS")

    # ─────────────────────────────────────────────
    # VERSÃO INCREMENTAL
    # ─────────────────────────────────────────────
    def get_next_version(self):
        try:
            r = requests.get(f"{self._repo_base()}/tags", headers=self._headers())
            if r.status_code == 200:
                tags = r.json()
                if not tags:
                    return "v1.0.0"
                versions = []
                for t in tags:
                    n = t['name']
                    if n.startswith('v') and n[1:].replace('.', '').isdigit():
                        parts = n[1:].split('.')
                        if len(parts) == 3:
                            versions.append([int(p) for p in parts])
                if versions:
                    versions.sort(reverse=True)
                    maj, minor, patch = versions[0]
                    return f"v{maj}.{minor}.{patch + 1}"
            return "v1.0.0"
        except Exception as e:
            self.log(f"Erro ao buscar versões: {e}", "ERROR")
            return "v1.0.0"

    # ─────────────────────────────────────────────
    # VERIFICAR TAG EXISTENTE
    # ─────────────────────────────────────────────
    def tag_exists(self, tag):
        try:
            r = requests.get(f"{self._repo_base()}/releases/tags/{tag}",
                             headers=self._headers(), timeout=10)
            return r.status_code == 200
        except Exception:
            return False

    # ─────────────────────────────────────────────
    # DELETAR RELEASE
    # ─────────────────────────────────────────────
    def delete_release(self, tag):
        try:
            r = requests.get(f"{self._repo_base()}/releases/tags/{tag}",
                             headers=self._headers())
            if r.status_code == 200:
                release_id = r.json()["id"]
                self.log(f"Deletando release '{tag}' (ID: {release_id})...", "DELETE")
                requests.delete(f"{self._repo_base()}/releases/{release_id}",
                                headers=self._headers())
                self.log("Release deletada.", "SUCCESS")
                self.log("Deletando tag...", "DELETE")
                requests.delete(f"{self._repo_base()}/git/refs/tags/{tag}",
                                headers=self._headers())
                self.log("Tag deletada.", "SUCCESS")
                import time; time.sleep(1)
            elif r.status_code == 404:
                self.log(f"Tag '{tag}' não existe ainda.", "INFO")
            return True
        except Exception as e:
            self.log(f"Aviso ao deletar: {e}", "WARNING")
            return True

    # ─────────────────────────────────────────────
    # BUILD FIRMWARE
    # ─────────────────────────────────────────────
    def _find_pio(self):
        """
        Busca o executável do PlatformIO em ordem de prioridade:
        1. .venv dentro da pasta do projeto selecionada
        2. ~/.platformio/penv/bin/pio  (instalação padrão do PlatformIO)
        3. ~/.local/bin/pio
        4. 'pio' no PATH do sistema
        """
        project_dir = self.project_dir.get().strip()

        candidatos = []

        # 1. venv do próprio projeto
        if project_dir:
            candidatos.append(Path(project_dir) / ".venv" / "bin" / "pio")

        # 2. instalação padrão do PlatformIO IDE
        candidatos.append(Path.home() / ".platformio" / "penv" / "bin" / "pio")

        # 3. instalação via pip --user
        candidatos.append(Path.home() / ".local" / "bin" / "pio")

        for c in candidatos:
            if c.exists():
                self.log(f"pio encontrado em: {c}", "INFO")
                return str(c)

        # 4. PATH do sistema
        self.log("pio não encontrado em caminhos conhecidos, tentando PATH...", "WARNING")
        return "pio"

    def build_firmware(self):
        try:
            project_dir = self.project_dir.get().strip()
            if not project_dir:
                self.log("Pasta do projeto não especificada!", "ERROR")
                return None
            if not Path(project_dir).exists():
                self.log(f"Pasta não encontrada: {project_dir}", "ERROR")
                return None
            if not (Path(project_dir) / "platformio.ini").exists():
                self.log("platformio.ini não encontrado!", "ERROR")
                return None

            env     = self.env_entry.get()
            pio_bin = self._find_pio()

            self.log("Compilando firmware...", "BUILD")
            result = subprocess.run(
                [pio_bin, "run", "--environment", env, "--project-dir", project_dir],
                capture_output=True, text=True, cwd=project_dir)

            if result.returncode != 0:
                self.log("ERRO NA COMPILAÇÃO:", "ERROR")
                for line in result.stderr.split('\n'):
                    if line.strip():
                        self.log(line, "ERROR")
                return None

            firmware_path = Path(project_dir) / ".pio" / "build" / env / "firmware.bin"
            if not firmware_path.exists():
                self.log(f"Firmware não encontrado em: {firmware_path}", "ERROR")
                return None

            size = firmware_path.stat().st_size
            self.log(f"Compilação OK — {size / 1024:.2f} KB", "SUCCESS")
            return str(firmware_path)

        except FileNotFoundError:
            self.log("PlatformIO não encontrado! (pip install platformio)", "ERROR")
            return None
        except Exception as e:
            self.log(f"Erro ao compilar: {e}", "ERROR")
            return None

    # ─────────────────────────────────────────────
    # CRIAR RELEASE
    # ─────────────────────────────────────────────
    def create_release(self, tag, firmware_path, description):
        try:
            self.log(f"Criando release '{tag}'...", "INFO")
            data = {
                "tag_name":   tag,
                "name":       f"Firmware {tag}",
                "body":       description,
                "draft":      False,
                "prerelease": False
            }
            r = requests.post(f"{self._repo_base()}/releases",
                              headers=self._headers(), json=data)
            if r.status_code != 201:
                err = r.json()
                self.log(f"Erro {r.status_code}: {err.get('message', '?')}", "ERROR")
                if r.status_code == 403:
                    self.log("Token sem permissão 'repo' completa.", "ERROR")
                return False
            upload_url = r.json()["upload_url"].replace("{?name,label}", "")
            self.log("Release criada. Enviando firmware...", "UPLOAD")
            with open(firmware_path, 'rb') as f:
                up = requests.post(
                    f"{upload_url}?name=firmware.bin",
                    headers={**self._headers(), "Content-Type": "application/octet-stream"},
                    data=f)
            if up.status_code == 201:
                self.log(
                    f"Firmware enviado! → https://github.com/"
                    f"{self.owner_entry.get()}/{self.repo_entry.get()}/releases/tag/{tag}",
                    "SUCCESS")
                return True
            else:
                self.log(f"Erro no upload: {up.status_code}", "ERROR")
                return False
        except Exception as e:
            self.log(f"Erro ao criar release: {e}", "ERROR")
            return False

    # ─────────────────────────────────────────────
    # PROCESSO COMPLETO
    # ─────────────────────────────────────────────
    def release_process(self):
        try:
            self.create_button.config(state="disabled")
            self.progress.start()

            mode = self.version_mode.get()
            if mode == "fixed":
                tag = "update"
            elif mode == "incremental":
                self.log("Buscando próxima versão...", "INFO")
                tag = self.get_next_version()
                self.log(f"Próxima versão: {tag}", "SUCCESS")
            else:
                tag = self.version_entry.get().strip()
                if not tag:
                    self.log("Tag não pode estar vazia!", "ERROR")
                    messagebox.showerror("Erro", "Digite uma tag/versão!")
                    return

            if self.tag_exists(tag):
                resposta = messagebox.askyesno(
                    "Tag já existe",
                    f"A release '{tag}' já existe no repositório.\n\n"
                    f"Deseja deletar a versão atual e substituí-la?\n\n"
                    f"(Isso apagará o firmware anterior da tag '{tag}')")
                if not resposta:
                    self.log(f"Operação cancelada — tag '{tag}' já existe.", "WARNING")
                    return
                self.log(f"Substituindo release '{tag}'...", "WARNING")
                self.delete_release(tag)

            if self.firmware_mode.get() == "build":
                if not self.project_dir.get().strip():
                    messagebox.showerror("Erro", "Selecione a pasta do projeto PlatformIO!")
                    return
                firmware_path = self.build_firmware()
                if not firmware_path:
                    messagebox.showerror("Erro na Compilação",
                                         "Falha ao compilar. Veja o log.")
                    return
            else:
                firmware_path = self.file_path.get()
                if not firmware_path or not Path(firmware_path).exists():
                    messagebox.showerror("Erro", "Selecione um arquivo .bin válido!")
                    return
                self.log(
                    f"Usando .bin: {Path(firmware_path).name} "
                    f"({Path(firmware_path).stat().st_size / 1024:.2f} KB)", "INFO")

            description = self.desc_text.get("1.0", tk.END).strip() or \
                f"Firmware {tag}\n\nAtualização automática."

            success = self.create_release(tag, firmware_path, description)

            if success:
                self.log("=" * 45, "SUCCESS")
                self.log("🎉 RELEASE CRIADA COM SUCESSO!", "SUCCESS")
                self.log("=" * 45, "SUCCESS")
                messagebox.showinfo("Sucesso! 🎉",
                    f"Release '{tag}' criada!\n\n"
                    f"✅ Firmware enviado\n"
                    f"✅ ESP32s podem atualizar via OTA!")
                self.load_releases_thread()
            else:
                messagebox.showerror("Erro",
                    "Falha ao criar release!\nVeja o log para detalhes.")

        except Exception as e:
            self.log(f"ERRO CRÍTICO: {e}", "ERROR")
            import traceback
            self.log(traceback.format_exc(), "ERROR")
            messagebox.showerror("Erro Crítico", str(e))
        finally:
            self.progress.stop()
            self.create_button.config(state="normal")
            self.log("Processo finalizado.", "INFO")

    def start_release_process(self):
        if not self.token_entry.get().strip():
            messagebox.showerror("Erro", "Token do GitHub é obrigatório!")
            return
        if not self.owner_entry.get().strip():
            messagebox.showerror("Erro", "Owner é obrigatório!")
            return
        if not self.repo_entry.get().strip():
            messagebox.showerror("Erro", "Repositório é obrigatório!")
            return
        if self.firmware_mode.get() == "file" and not self.file_path.get():
            messagebox.showerror("Erro", "Selecione um arquivo .bin!")
            return
        self.log("=" * 45, "INFO")
        self.log("🚀 INICIANDO PROCESSO DE RELEASE", "INFO")
        self.log("=" * 45, "INFO")
        threading.Thread(target=self.release_process, daemon=True).start()


def main():
    root = tk.Tk()
    style = ttk.Style()
    style.theme_use('clam')
    ESP32ReleaseManager(root)
    root.mainloop()


if __name__ == "__main__":
    main()