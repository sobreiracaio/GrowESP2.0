import firebase_admin
from firebase_admin import credentials, auth

# Carrega suas credenciais
cred = credentials.Certificate("growstation-183df-firebase-adminsdk-fbsvc-09e1606261.json")  # caminho pro seu arquivo JSON

# Inicializa o Firebase
firebase_admin.initialize_app(cred)

# Cria o usuário
user = auth.create_user(
    email="usuario@exemplo.com",
    password="senha123",
    display_name="João Silva"
)

print("Usuário criado com sucesso!")
print("UID:", user.uid)
print("Email:", user.email)