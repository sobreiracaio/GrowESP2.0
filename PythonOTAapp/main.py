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
                self.saved_project_dir = config.get('project_dir', '')
        else:
            self.saved_token = ''
            self.saved_owner = ''
            self.saved_repo = ''
            self.saved_env = 'esp32'
            self.saved_project_dir = ''
    
    def save_config(self):
        """Salva configurações"""
        config = {
            'token': self.token_entry.get(),
            'owner': self.owner_entry.get(),
            'repo': self.repo_entry.get(),
            'environment': self.env_entry.get(),
            'project_dir': self.project_dir.get()
        }
        with open(self.config_file, 'w') as f:
            json.dump(config, f, indent=2)
        
        messagebox.showinfo("Sucesso", "Configurações salvas!")
        self.log("Configurações salvas com sucesso", "SUCCESS")
    
    def select_project_dir(self):
        """Seleciona diretório do projeto PlatformIO"""
        directory = filedialog.askdirectory(
            title="Selecione a pasta do projeto PlatformIO",
            initialdir=self.project_dir.get() or os.path.expanduser("~")
        )
        
        if directory:
            # Verificar se tem platformio.ini
            ini_path = Path(directory) / "platformio.ini"
            if ini_path.exists():
                self.project_dir.set(directory)
                self.log(f"Pasta do projeto definida: {directory}", "SUCCESS")
            else:
                messagebox.showwarning(
                    "Aviso", 
                    f"A pasta selecionada não contém 'platformio.ini'!\n\n"
                    f"Procurado em: {ini_path}\n\n"
                    f"Deseja usar mesmo assim?"
                )
                response = messagebox.askyesno("Confirmar", "Usar esta pasta mesmo sem platformio.ini?")
                if response:
                    self.project_dir.set(directory)
    
    def test_connection(self):
        """Testa conexão com GitHub"""
        token = self.token_entry.get().strip()
        owner = self.owner_entry.get().strip()
        repo = self.repo_entry.get().strip()
        
        if not token or not owner or not repo:
            messagebox.showerror("Erro", "Preencha todos os campos antes de testar!")
            return
        
        self.log("Testando conexão com GitHub...", "INFO")
        self.progress.start()
        
        try:
            # Testar autenticação
            url = "https://api.github.com/user"
            headers = {"Authorization": f"token {token}"}
            response = requests.get(url, headers=headers, timeout=10)
            
            if response.status_code == 200:
                user_data = response.json()
                self.log(f"✅ Token válido! Usuário: {user_data.get('login')}", "SUCCESS")
            else:
                self.log(f"❌ Token inválido! Status: {response.status_code}", "ERROR")
                messagebox.showerror("Erro", f"Token inválido!\n\nStatus: {response.status_code}\n\nVerifique se o token tem permissão 'repo'")
                self.progress.stop()
                return
            
            # Testar acesso ao repositório
            url = f"https://api.github.com/repos/{owner}/{repo}"
            response = requests.get(url, headers=headers, timeout=10)
            
            if response.status_code == 200:
                repo_data = response.json()
                self.log(f"✅ Repositório encontrado: {repo_data.get('full_name')}", "SUCCESS")
                self.log(f"   Privado: {'Sim' if repo_data.get('private') else 'Não'}", "INFO")
                messagebox.showinfo(
                    "Sucesso! ✅", 
                    f"Conexão estabelecida!\n\n"
                    f"Usuário: {user_data.get('login')}\n"
                    f"Repositório: {repo_data.get('full_name')}\n"
                    f"Privado: {'Sim' if repo_data.get('private') else 'Não'}"
                )
            elif response.status_code == 404:
                self.log(f"❌ Repositório não encontrado!", "ERROR")
                messagebox.showerror("Erro", f"Repositório '{owner}/{repo}' não encontrado!\n\nVerifique se:\n- O nome está correto\n- Você tem acesso ao repositório")
            else:
                self.log(f"❌ Erro ao acessar repositório: {response.status_code}", "ERROR")
                messagebox.showerror("Erro", f"Erro ao acessar repositório!\n\nStatus: {response.status_code}")
        
        except requests.exceptions.Timeout:
            self.log("❌ Timeout - sem conexão com internet?", "ERROR")
            messagebox.showerror("Erro", "Timeout!\n\nVerifique sua conexão com a internet.")
        except Exception as e:
            self.log(f"❌ Erro ao testar: {e}", "ERROR")
            messagebox.showerror("Erro", f"Erro ao testar conexão:\n\n{e}")
        finally:
            self.progress.stop()
    
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
        
        # Diretório do projeto PlatformIO
        ttk.Label(config_frame, text="Pasta do Projeto:").grid(row=4, column=0, sticky="w", pady=2)
        project_dir_frame = ttk.Frame(config_frame)
        project_dir_frame.grid(row=4, column=1, pady=2, padx=5, sticky="ew")
        
        self.project_dir = tk.StringVar()
        self.project_dir.set(self.saved_project_dir)
        ttk.Entry(project_dir_frame, textvariable=self.project_dir, state="readonly").pack(side="left", fill="x", expand=True, padx=(0, 5))
        ttk.Button(project_dir_frame, text="📁", width=3, command=self.select_project_dir).pack(side="left")
        
        # Botão salvar config
        btn_frame = ttk.Frame(config_frame)
        btn_frame.grid(row=5, column=0, columnspan=2, pady=10)
        
        ttk.Button(btn_frame, text="💾 Salvar Configurações", 
                  command=self.save_config).pack(side="left", padx=5)
        
        ttk.Button(btn_frame, text="🔍 Testar Conexão", 
                  command=self.test_connection).pack(side="left", padx=5)
        
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
            project_dir = self.project_dir.get().strip()
            
            # Verificar se diretório foi especificado
            if not project_dir:
                self.log("Pasta do projeto não foi especificada!", "ERROR")
                self.log("Clique no botão 📁 ao lado de 'Pasta do Projeto' para selecionar", "WARNING")
                return None
            
            # Verificar se diretório existe
            if not Path(project_dir).exists():
                self.log(f"Pasta não encontrada: {project_dir}", "ERROR")
                return None
            
            # Verificar se tem platformio.ini
            ini_path = Path(project_dir) / "platformio.ini"
            if not ini_path.exists():
                self.log(f"platformio.ini não encontrado em: {project_dir}", "ERROR")
                self.log("Verifique se selecionou a pasta correta do projeto!", "WARNING")
                return None
            
            self.log(f"Pasta do projeto: {project_dir}", "INFO")
            self.log("Iniciando compilação do firmware...", "BUILD")
            
            env = self.env_entry.get()
            
            # Executar PlatformIO no diretório correto
            result = subprocess.run(
                ["pio", "run", "--environment", env, "--project-dir", project_dir],
                capture_output=True,
                text=True,
                cwd=project_dir  # Executar a partir da pasta do projeto
            )
            
            if result.returncode != 0:
                self.log("=" * 50, "ERROR")
                self.log("ERRO NA COMPILAÇÃO:", "ERROR")
                self.log("=" * 50, "ERROR")
                
                # Extrair mensagem de erro específica
                stderr = result.stderr
                
                if "NotPlatformIOProjectError" in stderr:
                    self.log("❌ Não é um projeto PlatformIO válido", "ERROR")
                    self.log(f"   Pasta: {project_dir}", "ERROR")
                    self.log(f"   Verifique se existe 'platformio.ini' nesta pasta", "ERROR")
                elif "UnknownEnvNames" in stderr:
                    self.log(f"❌ Environment '{env}' não encontrado no platformio.ini", "ERROR")
                    self.log("   Verifique o nome do environment no arquivo platformio.ini", "ERROR")
                elif "PlatformioException" in stderr:
                    self.log("❌ Erro do PlatformIO", "ERROR")
                elif "command not found" in stderr.lower() or "pio" in stderr.lower():
                    self.log("❌ PlatformIO não está instalado ou não está no PATH", "ERROR")
                    self.log("   Instale com: pip install platformio", "ERROR")
                else:
                    self.log("❌ Erro desconhecido na compilação", "ERROR")
                
                # Mostrar saída completa do erro
                self.log("\n--- Saída de Erro Completa ---", "ERROR")
                for line in stderr.split('\n'):
                    if line.strip():
                        self.log(line, "ERROR")
                
                # Mostrar stdout também (pode ter info útil)
                if result.stdout:
                    self.log("\n--- Saída Padrão ---", "INFO")
                    for line in result.stdout.split('\n')[-20:]:  # Últimas 20 linhas
                        if line.strip():
                            self.log(line, "INFO")
                
                return None
            
            # Sucesso! Procurar o firmware
            firmware_path = Path(project_dir) / ".pio" / "build" / env / "firmware.bin"
            
            if not firmware_path.exists():
                self.log(f"Firmware não encontrado em: {firmware_path}", "ERROR")
                self.log("A compilação parece ter sucesso, mas o arquivo não foi gerado", "WARNING")
                return None
            
            size = firmware_path.stat().st_size
            self.log("=" * 50, "SUCCESS")
            self.log("✅ COMPILAÇÃO CONCLUÍDA!", "SUCCESS")
            self.log("=" * 50, "SUCCESS")
            self.log(f"Arquivo: {firmware_path.name}", "SUCCESS")
            self.log(f"Tamanho: {size:,} bytes ({size/1024:.2f} KB)", "SUCCESS")
            
            return str(firmware_path)
            
        except FileNotFoundError:
            self.log("=" * 50, "ERROR")
            self.log("❌ PlatformIO não encontrado!", "ERROR")
            self.log("=" * 50, "ERROR")
            self.log("PlatformIO CLI não está instalado ou não está no PATH", "ERROR")
            self.log("\nPara instalar:", "INFO")
            self.log("  pip install platformio", "INFO")
            self.log("\nOu use: python -m pip install platformio", "INFO")
            return None
        except Exception as e:
            self.log(f"❌ Erro inesperado ao compilar: {e}", "ERROR")
            import traceback
            self.log(traceback.format_exc(), "ERROR")
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
            
            # Buscar release pela tag
            url = f"https://api.github.com/repos/{owner}/{repo}/releases/tags/{tag}"
            response = requests.get(url, headers=headers)
            
            if response.status_code == 200:
                release_id = response.json()["id"]
                self.log(f"Release '{tag}' encontrada (ID: {release_id})", "INFO")
                self.log("Deletando release antiga...", "DELETE")
                
                # Deletar release
                delete_url = f"https://api.github.com/repos/{owner}/{repo}/releases/{release_id}"
                del_response = requests.delete(delete_url, headers=headers)
                
                if del_response.status_code == 204:
                    self.log("Release deletada com sucesso!", "SUCCESS")
                else:
                    self.log(f"Aviso ao deletar release: {del_response.status_code}", "WARNING")
                    if del_response.status_code == 403:
                        self.log("Token não tem permissão para deletar releases", "WARNING")
                        self.log("Verifique se o token tem permissão 'repo' completa", "WARNING")
                
                # Deletar tag
                self.log("Deletando tag...", "DELETE")
                tag_url = f"https://api.github.com/repos/{owner}/{repo}/git/refs/tags/{tag}"
                tag_response = requests.delete(tag_url, headers=headers)
                
                if tag_response.status_code == 204:
                    self.log("Tag deletada com sucesso!", "SUCCESS")
                else:
                    self.log(f"Aviso ao deletar tag: {tag_response.status_code}", "WARNING")
                
                # Aguardar um pouco para o GitHub processar
                import time
                time.sleep(1)
                
            elif response.status_code == 404:
                self.log(f"Release '{tag}' não existe (ok, será criada)", "INFO")
            else:
                self.log(f"Aviso ao verificar release: {response.status_code}", "WARNING")
            
            return True
            
        except Exception as e:
            self.log(f"Aviso ao deletar release: {e}", "WARNING")
            return True  # Continuar mesmo com erro
    
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
                self.log("=" * 50, "ERROR")
                self.log(f"❌ Erro ao criar release: {response.status_code}", "ERROR")
                self.log("=" * 50, "ERROR")
                
                error_data = response.json()
                error_msg = error_data.get('message', 'Erro desconhecido')
                
                self.log(f"Mensagem: {error_msg}", "ERROR")
                
                # Erros específicos
                if response.status_code == 403:
                    self.log("\n🔒 ERRO 403: Permissão Negada", "ERROR")
                    self.log("", "ERROR")
                    self.log("Possíveis causas:", "ERROR")
                    self.log("1. Token sem permissão 'repo' completa", "ERROR")
                    self.log("2. Token expirado ou inválido", "ERROR")
                    self.log("3. Repositório não permite seu token", "ERROR")
                    self.log("", "ERROR")
                    self.log("✅ SOLUÇÃO:", "INFO")
                    self.log("1. Vá em: https://github.com/settings/tokens", "INFO")
                    self.log("2. Crie um NOVO token (Personal Access Token - Classic)", "INFO")
                    self.log("3. Marque TODO o checkbox 'repo' (não apenas 'public_repo')", "INFO")
                    self.log("4. Copie o novo token e cole no campo 'Token do GitHub'", "INFO")
                    self.log("5. Clique em 'Salvar Configurações'", "INFO")
                    
                elif response.status_code == 422:
                    self.log("\n⚠️ ERRO 422: Validação Falhou", "ERROR")
                    
                    if "already_exists" in error_msg.lower() or "tag name already exists" in error_msg.lower():
                        self.log("", "ERROR")
                        self.log("A tag/release já existe!", "ERROR")
                        self.log("", "ERROR")
                        self.log("Isso NÃO deveria acontecer porque tentamos deletar antes.", "WARNING")
                        self.log("", "WARNING")
                        self.log("✅ SOLUÇÕES:", "INFO")
                        self.log("1. Delete manualmente no GitHub:", "INFO")
                        self.log(f"   https://github.com/{owner}/{repo}/releases", "INFO")
                        self.log("2. Ou use outro nome de tag/versão", "INFO")
                    else:
                        self.log(f"Detalhes: {error_msg}", "ERROR")
                
                elif response.status_code == 404:
                    self.log("\n❌ ERRO 404: Repositório não encontrado", "ERROR")
                    self.log(f"Verificando: {owner}/{repo}", "ERROR")
                    self.log("Confira se o nome está correto!", "WARNING")
                
                # Mostrar resposta completa
                self.log("\n--- Resposta Completa da API ---", "ERROR")
                self.log(json.dumps(error_data, indent=2), "ERROR")
                
                return False
            
            release_data = response.json()
            upload_url = release_data["upload_url"].replace("{?name,label}", "")
            
            self.log("✅ Release criada com sucesso!", "SUCCESS")
            
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
                self.log("✅ Firmware enviado com sucesso!", "SUCCESS")
                self.log(f"🔗 URL: https://github.com/{owner}/{repo}/releases/tag/{tag}", "INFO")
                return True
            else:
                self.log(f"❌ Erro no upload: {upload_response.status_code}", "ERROR")
                self.log(upload_response.json(), "ERROR")
                return False
            
        except Exception as e:
            self.log(f"❌ Erro ao criar release: {e}", "ERROR")
            import traceback
            self.log(traceback.format_exc(), "ERROR")
            return False
    
    def release_process(self):
        """Processo completo de release"""
        try:
            self.create_button.config(state="disabled")
            self.progress.start()
            
            self.log("Validando configurações...", "INFO")
            
            # 1. Determinar tag/versão
            mode = self.version_mode.get()
            
            if mode == "fixed":
                tag = "update"
                self.log(f"Modo: Tag fixa ('{tag}')", "INFO")
            elif mode == "incremental":
                self.log("Buscando próxima versão...", "INFO")
                tag = self.get_next_version()
                self.log(f"Próxima versão: {tag}", "SUCCESS")
            else:  # manual
                tag = self.version_entry.get().strip()
                if not tag:
                    self.log("Tag/versão não pode estar vazia!", "ERROR")
                    messagebox.showerror("Erro", "Digite uma tag/versão!")
                    return
                self.log(f"Modo: Versão manual ('{tag}')", "INFO")
            
            # 2. Obter firmware
            firmware_path = None
            
            if self.firmware_mode.get() == "build":
                self.log("Modo: Compilar com PlatformIO", "INFO")
                
                # Verificar se pasta do projeto está configurada
                if not self.project_dir.get().strip():
                    self.log("❌ Pasta do projeto não configurada!", "ERROR")
                    messagebox.showerror(
                        "Erro", 
                        "Pasta do projeto não foi especificada!\n\n"
                        "Clique no botão 📁 ao lado de 'Pasta do Projeto'\n"
                        "e selecione a pasta que contém o platformio.ini"
                    )
                    return
                
                firmware_path = self.build_firmware()
                if not firmware_path:
                    messagebox.showerror(
                        "Erro na Compilação", 
                        "Falha ao compilar firmware!\n\n"
                        "Verifique o LOG para mais detalhes.\n\n"
                        "Problemas comuns:\n"
                        "• PlatformIO não instalado (pip install platformio)\n"
                        "• Pasta do projeto incorreta\n"
                        "• Environment incorreto no platformio.ini\n"
                        "• Erros no código fonte"
                    )
                    return
            else:
                self.log("Modo: Usar arquivo .bin selecionado", "INFO")
                firmware_path = self.file_path.get()
                if not firmware_path or not Path(firmware_path).exists():
                    self.log("Arquivo .bin não encontrado!", "ERROR")
                    messagebox.showerror("Erro", "Selecione um arquivo .bin válido!")
                    return
                
                size = Path(firmware_path).stat().st_size
                self.log(f"Firmware selecionado: {Path(firmware_path).name}", "INFO")
                self.log(f"Tamanho: {size:,} bytes ({size/1024:.2f} KB)", "INFO")
            
            # 3. Deletar release antiga (se modo fixo)
            if mode == "fixed":
                self.log("Modo tag fixa: verificando release anterior...", "INFO")
                if not self.delete_release(tag):
                    self.log("Erro ao deletar release anterior", "WARNING")
                    response = messagebox.askyesno(
                        "Continuar?",
                        "Não foi possível deletar a release anterior.\n\n"
                        "Isso pode causar erro 422 (já existe).\n\n"
                        "Deseja continuar mesmo assim?"
                    )
                    if not response:
                        return
            
            # 4. Criar release
            self.log("Preparando para criar release...", "INFO")
            description = self.desc_text.get("1.0", tk.END).strip()
            
            if not description:
                description = f"Firmware {tag}\n\nAtualização automática via Release Manager"
            
            success = self.create_release(tag, firmware_path, description)
            
            if success:
                self.log("=" * 50, "SUCCESS")
                self.log("🎉 RELEASE CRIADA COM SUCESSO! 🎉", "SUCCESS")
                self.log("=" * 50, "SUCCESS")
                
                owner = self.owner_entry.get()
                repo = self.repo_entry.get()
                url = f"https://github.com/{owner}/{repo}/releases/tag/{tag}"
                
                messagebox.showinfo(
                    "Sucesso! 🎉", 
                    f"Release '{tag}' criada com sucesso!\n\n"
                    f"✅ Firmware enviado\n"
                    f"✅ Seus ESP32s já podem atualizar via OTA!\n\n"
                    f"URL: {url}"
                )
            else:
                messagebox.showerror(
                    "Erro", 
                    "Falha ao criar release!\n\n"
                    "Verifique:\n"
                    "- Token está correto e tem permissão 'repo'\n"
                    "- Owner e Repositório estão corretos\n"
                    "- Veja o log para mais detalhes"
                )
            
        except Exception as e:
            self.log(f"ERRO CRÍTICO: {e}", "ERROR")
            import traceback
            self.log(traceback.format_exc(), "ERROR")
            messagebox.showerror("Erro Crítico", f"Erro inesperado:\n\n{e}\n\nVeja o log para detalhes.")
        
        finally:
            self.progress.stop()
            self.create_button.config(state="normal")
            self.log("Processo finalizado.", "INFO")
    
    def start_release_process(self):
        """Inicia processo em thread separada"""
        # Validar campos obrigatórios
        if not self.token_entry.get().strip():
            messagebox.showerror("Erro", "Token do GitHub é obrigatório!")
            return
        
        if not self.owner_entry.get().strip():
            messagebox.showerror("Erro", "Owner (usuário) é obrigatório!")
            return
        
        if not self.repo_entry.get().strip():
            messagebox.showerror("Erro", "Repositório é obrigatório!")
            return
        
        # Se modo arquivo, validar seleção
        if self.firmware_mode.get() == "file" and not self.file_path.get():
            messagebox.showerror("Erro", "Selecione um arquivo .bin!")
            return
        
        self.log("=" * 50, "INFO")
        self.log("🚀 INICIANDO PROCESSO DE RELEASE", "INFO")
        self.log("=" * 50, "INFO")
        
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