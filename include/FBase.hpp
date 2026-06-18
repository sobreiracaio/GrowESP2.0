#pragma once

#include "Libraries.hpp"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "Secrets.h"   // DATABASE_URL e API_KEY ficam aqui, fora do controle de versão — ver Secrets.h.example

#define AUTH_URL     "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key="
#define REFRESH_URL  "https://securetoken.googleapis.com/v1/token?key="

#define INT    0
#define FLOAT  1
#define STRING 2
#define BOOL   3

class FBase {
public:
    FBase(const String& api, const String& db_url,
          const String& user_email, const String& user_password);

    bool init();           // autentica e obtém idToken
    bool stopApp();        // limpa estado
    void safeStop();       // compatibilidade
    bool isReady();        // token válido e WiFi ok
    bool isHealthy();      // mesma coisa
    void loop();           // renova token se necessário
    bool isBusy();         // sempre false — REST é síncrono
    bool hasCredentialError();

    // GET — retorna o valor como String, vazio se falhou
    bool dbGet(const String& path, String& result);

    // GET raw — retorna JSON bruto sem remover aspas (para nós com filhos)
    bool dbGetRaw(const String& path, String& result);

    // SET — envia valor para o path
    bool dbSet(const String& path, const String& value);
    bool dbSet(const String& path, float value);
    bool dbSet(const String& path, bool value);

    // Compatibilidade com código existente
    void awaitGet(String& path, String* result);
    void awaitGet(const char* path, char* result, unsigned int resultSize);

    void aSyncSetString(String path, String value);
    void aSyncSetString(const char* path, const char* value);
    void aSyncSetFloat(String path, float value);
    void aSyncSetFloat(const char* path, float value);
    void aSyncSetBool(String path, bool value);
    void aSyncSetBool(const char* path, bool value);

    void aSyncGet(String& path, String& result);
    void awaitSet(String& path, String value, int type);

    bool hasCredentialError() const { return _credentialError; }

private:
    String _apiKey;
    String _dbUrl;
    String _email;
    String _password;

    String _idToken;
    String _refreshToken;
    unsigned long _tokenExpiry = 0;   // millis() quando expira

    bool _ready           = false;
    bool _credentialError = false;

    bool authenticate();
    bool refreshToken();
    bool httpGet(const String& url, String& response);
    bool httpPut(const String& url, const String& body, String& response);

    String buildUrl(const String& path) const;
    bool   ensureToken();
};