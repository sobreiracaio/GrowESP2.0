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
        self.root.geometry("700x800")
        self.root.resizable(True, True)
        
        # Configurações
        self.config_file = "release_config.json"
        self.load_config()
        
        self.create_widgets()
        
    def load_config(self):
        """Carrega configurações salvas"""
        if Path(self.config_file).exists():
            with open(self.config_file, 'r') as f:
                config = json.load(f)
                self.saved_token = config.get('token', '')
                self.saved_owner = config.get('owner', '')
                self.saved_repo = config.get('repo', '')
                self.saved_env = config.get('environment', 'esp32')
        else:
            self.saved_token = ''
            self.saved_owner = ''
            self.saved_repo = ''
            self.saved_env = 'esp32'
    
    def save_config(self):
        """Salva configurações"""
        config = {
            'token': self.token_entry.get(),
            'owner': self.owner_entry.get(),
            'repo': self.repo_entry.get(),
            'environment': self.env_entry.get()
        }
        with open(self.config_file, 'w') as f:
            json.dump(config, f, indent=2)
    
    def create_widgets(self):
        """Cria a interface gráfica"""
        
        # =========================
        # FRAME: Configurações
        # =========================
        config_frame = ttk.LabelFrame(self.root, text="⚙️  Configurações do GitHub", padding=10)
        config_frame.pack(fill="x", padx=10, pady=5)
        
        # Token
        ttk.Label(config_frame, text="Token do GitHub:").grid(row=0, column=0, sticky="w", pady=2)
        self.token_entry = ttk.Entry(config_frame, width=50, show="*")
        self.token_entry.grid(row=0, column=1, pady=2, padx=5)
        self.token_entry.insert(0, self.saved_token)
        
        # Owner
        ttk.Label(config_frame, text="Owner (usuário):").grid(row=1, column=0, sticky="w", pady=2)
        self.owner_entry = ttk.Entry(config_frame, width=50)
        self.owner_entry.grid(row=1, column=1, pady=2, padx=5)
        self.owner_entry.insert(0, self.saved_owner)
        
        # Repositório
        ttk.Label(config_frame, text="Repositório:").grid(row=2, column=0, sticky="w", pady=2)
        self.repo_entry = ttk.Entry(config_frame, width=50)
        self.repo_entry.grid(row=2, column=1, pady=2, padx=5)
        self.repo_entry.insert(0, self.saved_repo)
        
        # Environment PlatformIO
        ttk.Label(config_frame, text="PlatformIO Env:").grid(row=3, column=0, sticky="w", pady=2)
        self.env_entry = ttk.Entry(config_frame, width=50)
        self.env_entry.grid(row=3, column=1, pady=2, padx=5)
        self.env_entry.insert(0, self.saved_env)
        
        # Botão salvar config
        ttk.Button(config_frame, text="💾 Salvar Configurações", 
                  command=self.save_config).grid(row=4, column=0, columnspan=2, pady=10)
        
        # =========================
        # FRAME: Modo de Versionamento
        # =========================
        version_frame = ttk.LabelFrame(self.root, text="📋 Modo de Versionamento", padding=10)
        version_frame.pack(fill="x", padx=10, pady=5)
        
        self.version_mode = tk.StringVar(value="fixed")
        
        ttk.Radiobutton(version_frame, text="Tag Fixa (sempre 'update')", 
                       variable=self.version_mode, value="fixed",
                       command=self.toggle_version_mode).pack(anchor="w", pady=2)
        
        ttk.Radiobutton(version_frame, text="Versão Incremental (v1.0.0, v1.0.1...)", 
                       variable=self.version_mode, value="incremental",
                       command=self.toggle_version_mode).pack(anchor="w", pady=2)
        
        ttk.Radiobutton(version_frame, text="Versão Manual (você escolhe)", 
                       variable=self.version_mode, value="manual",
                       command=self.toggle_version_mode).pack(anchor="w", pady=2)
        
        # Campo de versão manual
        version_input_frame = ttk.Frame(version_frame)
        version_input_frame.pack(fill="x", pady=5)
        
        ttk.Label(version_input_frame, text="Tag/Versão:").pack(side="left", padx=5)
        self.version_entry = ttk.Entry(version_input_frame, width=20)
        self.version_entry.pack(side="left", padx=5)
        self.version_entry.insert(0, "update")
        self.version_entry.config(state="disabled")
        
        # =========================
        # FRAME: Firmware
        # =========================
        firmware_frame = ttk.LabelFrame(self.root, text="📦 Firmware", padding=10)
        firmware_frame.pack(fill="x", padx=10, pady=5)
        
        self.firmware_mode = tk.StringVar(value="build")
        
        ttk.Radiobutton(firmware_frame, text="Compilar com PlatformIO", 
                       variable=self.firmware_mode, value="build").pack(anchor="w", pady=2)
        
        ttk.Radiobutton(firmware_frame, text="Selecionar arquivo .bin existente", 
                       variable=self.firmware_mode, value="file").pack(anchor="w", pady=2)
        
        # Seletor de arquivo
        file_frame = ttk.Frame(firmware_frame)
        file_frame.pack(fill="x", pady=5)
        
        self.file_path = tk.StringVar()
        ttk.Entry(file_frame, textvariable=self.file_path, state="readonly", width=40).pack(side="left", padx=5)
        ttk.Button(file_frame, text="📂 Selecionar", command=self.select_file).pack(side="left")
        
        # =========================
        # FRAME: Descrição da Release
        # =========================
        desc_frame = ttk.LabelFrame(self.root, text="📝 Descrição da Release", padding=10)
        desc_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.desc_text = scrolledtext.ScrolledText(desc_frame, height=4, wrap=tk.WORD)
        self.desc_text.pack(fill="both", expand=True)
        self.desc_text.insert("1.0", "Atualização automática do firmware\n\nAlterações:\n- ")
        
        # =========================
        # BOTÃO PRINCIPAL
        # =========================
        self.create_button = ttk.Button(self.root, text="🚀 CRIAR RELEASE", 
                                       command=self.start_release_process,
                                       style="Accent.TButton")
        self.create_button.pack(pady=10, ipadx=20, ipady=10)
        
        # =========================
        # LOG
        # =========================
        log_frame = ttk.LabelFrame(self.root, text="📊 Log de Atividades", padding=10)
        log_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        self.log_text = scrolledtext.ScrolledText(log_frame, height=10, state="disabled",
                                                  bg="#1e1e1e", fg="#00ff00", 
                                                  font=("Courier", 9))
        self.log_text.pack(fill="both", expand=True)
        
        # Barra de progresso
        self.progress = ttk.Progressbar(self.root, mode="indeterminate")
        self.progress.pack(fill="x", padx=10, pady=5)
        
    def toggle_version_mode(self):
        """Habilita/desabilita campo de versão"""
        if self.version_mode.get() == "manual":
            self.version_entry.config(state="normal")
        else:
            self.version_entry.config(state="disabled")
    
    def select_file(self):
        """Seleciona arquivo .bin"""
        filename = filedialog.askopenfilename(
            title="Selecione o firmware",
            filetypes=[("Arquivos BIN", "*.bin"), ("Todos os arquivos", "*.*")]
        )
        if filename:
            self.file_path.set(filename)
    
    def log(self, message, level="INFO"):
        """Adiciona mensagem ao log"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        
        icons = {
            "INFO": "ℹ️",
            "SUCCESS": "✅",
            "ERROR": "❌",
            "WARNING": "⚠️",
            "BUILD": "🔨",
            "UPLOAD": "📤",
            "DELETE": "🗑️"
        }
        
        icon = icons.get(level, "•")
        log_message = f"[{timestamp}] {icon} {message}\n"
        
        self.log_text.config(state="normal")
        self.log_text.insert(tk.END, log_message)
        self.log_text.see(tk.END)
        self.log_text.config(state="disabled")
        self.root.update()
    
    def get_next_version(self):
        """Obtém a próxima versão incremental"""
        try:
            url = f"https://api.github.com/repos/{self.owner_entry.get()}/{self.repo_entry.get()}/tags"
            headers = {"Authorization": f"token {self.token_entry.get()}"}
            
            response = requests.get(url, headers=headers)
            
            if response.status_code == 200:
                tags = response.json()
                
                if not tags:
                    return "v1.0.0"
                
                # Procurar última versão v*.*.* 
                versions = []
                for tag in tags:
                    name = tag['name']
                    if name.startswith('v') and name[1:].replace('.', '').isdigit():
                        parts = name[1:].split('.')
                        if len(parts) == 3:
                            versions.append([int(p) for p in parts])
                
                if versions:
                    versions.sort(reverse=True)
                    major, minor, patch = versions[0]
                    patch += 1
                    return f"v{major}.{minor}.{patch}"
            
            return "v1.0.0"
            
        except Exception as e:
            self.log(f"Erro ao buscar versões: {e}", "ERROR")
            return "v1.0.0"
    
    def build_firmware(self):
        """Compila o firmware com PlatformIO"""
        try:
            self.log("Iniciando compilação do firmware...", "BUILD")
            
            env = self.env_entry.get()
            result = subprocess.run(
                ["pio", "run", "--environment", env],
                capture_output=True,
                text=True
            )
            
            if result.returncode != 0:
                self.log(f"Erro na compilação:\n{result.stderr}", "ERROR")
                return None
            
            firmware_path = f".pio/build/{env}/firmware.bin"
            
            if not Path(firmware_path).exists():
                self.log(f"Firmware não encontrado em: {firmware_path}", "ERROR")
                return None
            
            size = Path(firmware_path).stat().st_size
            self.log(f"Compilação concluída! Tamanho: {size:,} bytes ({size/1024:.2f} KB)", "SUCCESS")
            
            return firmware_path
            
        except FileNotFoundError:
            self.log("PlatformIO não encontrado! Instale com: pip install platformio", "ERROR")
            return None
        except Exception as e:
            self.log(f"Erro ao compilar: {e}", "ERROR")
            return None
    
    def delete_release(self, tag):
        """Deleta release existente"""
        try:
            owner = self.owner_entry.get()
            repo = self.repo_entry.get()
            token = self.token_entry.get()
            
            headers = {
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json"
            }
            
            # Buscar release
            url = f"https://api.github.com/repos/{owner}/{repo}/releases/tags/{tag}"
            response = requests.get(url, headers=headers)
            
            if response.status_code == 200:
                release_id = response.json()["id"]
                self.log(f"Deletando release antiga (ID: {release_id})...", "DELETE")
                
                # Deletar release
                delete_url = f"https://api.github.com/repos/{owner}/{repo}/releases/{release_id}"
                requests.delete(delete_url, headers=headers)
                
                # Deletar tag
                tag_url = f"https://api.github.com/repos/{owner}/{repo}/git/refs/tags/{tag}"
                requests.delete(tag_url, headers=headers)
                
                self.log("Release antiga deletada!", "SUCCESS")
            
        except Exception as e:
            self.log(f"Aviso ao deletar release: {e}", "WARNING")
    
    def create_release(self, tag, firmware_path, description):
        """Cria release no GitHub"""
        try:
            owner = self.owner_entry.get()
            repo = self.repo_entry.get()
            token = self.token_entry.get()
            
            self.log(f"Criando release '{tag}'...", "INFO")
            
            # Criar release
            url = f"https://api.github.com/repos/{owner}/{repo}/releases"
            headers = {
                "Authorization": f"token {token}",
                "Accept": "application/vnd.github.v3+json"
            }
            
            data = {
                "tag_name": tag,
                "name": f"Firmware {tag}",
                "body": description,
                "draft": False,
                "prerelease": False
            }
            
            response = requests.post(url, headers=headers, json=data)
            
            if response.status_code != 201:
                self.log(f"Erro ao criar release: {response.status_code}", "ERROR")
                self.log(response.json().get('message', 'Erro desconhecido'), "ERROR")
                return False
            
            release_data = response.json()
            upload_url = release_data["upload_url"].replace("{?name,label}", "")
            
            self.log("Release criada com sucesso!", "SUCCESS")
            
            # Upload do firmware
            self.log("Fazendo upload do firmware...", "UPLOAD")
            
            with open(firmware_path, 'rb') as f:
                upload_response = requests.post(
                    f"{upload_url}?name=firmware.bin",
                    headers={
                        "Authorization": f"token {token}",
                        "Content-Type": "application/octet-stream"
                    },
                    data=f
                )
            
            if upload_response.status_code == 201:
                self.log("Firmware enviado com sucesso!", "SUCCESS")
                self.log(f"URL: https://github.com/{owner}/{repo}/releases/tag/{tag}", "INFO")
                return True
            else:
                self.log(f"Erro no upload: {upload_response.status_code}", "ERROR")
                return False
            
        except Exception as e:
            self.log(f"Erro ao criar release: {e}", "ERROR")
            return False
    
    def release_process(self):
        """Processo completo de release"""
        try:
            self.create_button.config(state="disabled")
            self.progress.start()
            
            # 1. Determinar tag/versão
            mode = self.version_mode.get()
            
            if mode == "fixed":
                tag = "update"
            elif mode == "incremental":
                tag = self.get_next_version()
                self.log(f"Próxima versão: {tag}", "INFO")
            else:  # manual
                tag = self.version_entry.get().strip()
                if not tag:
                    self.log("Tag/versão não pode estar vazia!", "ERROR")
                    return
            
            # 2. Obter firmware
            if self.firmware_mode.get() == "build":
                firmware_path = self.build_firmware()
                if not firmware_path:
                    return
            else:
                firmware_path = self.file_path.get()
                if not firmware_path or not Path(firmware_path).exists():
                    self.log("Selecione um arquivo .bin válido!", "ERROR")
                    return
                
                size = Path(firmware_path).stat().st_size
                self.log(f"Firmware selecionado: {size:,} bytes ({size/1024:.2f} KB)", "INFO")
            
            # 3. Deletar release antiga (se modo fixo)
            if mode == "fixed":
                self.delete_release(tag)
            
            # 4. Criar release
            description = self.desc_text.get("1.0", tk.END).strip()
            success = self.create_release(tag, firmware_path, description)
            
            if success:
                self.log("=" * 50, "SUCCESS")
                self.log("🎉 RELEASE CRIADA COM SUCESSO! 🎉", "SUCCESS")
                self.log("=" * 50, "SUCCESS")
                messagebox.showinfo("Sucesso!", f"Release '{tag}' criada com sucesso!\n\nSeus ESP32s já podem atualizar via OTA!")
            
        except Exception as e:
            self.log(f"Erro no processo: {e}", "ERROR")
            messagebox.showerror("Erro", f"Erro ao criar release:\n{e}")
        
        finally:
            self.progress.stop()
            self.create_button.config(state="normal")
    
    def start_release_process(self):
        """Inicia processo em thread separada"""
        thread = threading.Thread(target=self.release_process, daemon=True)
        thread.start()

def main():
    root = tk.Tk()
    
    # Estilo
    style = ttk.Style()
    style.theme_use('clam')
    
    app = ESP32ReleaseManager(root)
    root.mainloop()

if __name__ == "__main__":
    main()