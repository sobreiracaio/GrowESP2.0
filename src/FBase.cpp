#include "FBase.hpp"

// ── Construtor ────────────────────────────────────────────────────────────────
FBase::FBase(const String& api, const String& db_url,
             const String& user_email, const String& user_password)
    : _apiKey(api), _dbUrl(db_url), _email(user_email), _password(user_password)
{}

// ── URL helper ────────────────────────────────────────────────────────────────
String FBase::buildUrl(const String& path) const
{
    // Garante exatamente uma barra entre _dbUrl e path
    String base = _dbUrl;
    if (base.length() > 0 && base[base.length()-1] != '/') base += '/';
    String p = path;
    if (p.length() > 0 && p[0] == '/') p = p.substring(1);
    return base + p + ".json?auth=" + _idToken;
}

// ── HTTP GET ──────────────────────────────────────────────────────────────────
bool FBase::httpGet(const String& url, String& response)
{
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    HTTPClient https;
    if (!https.begin(client, url)) return false;

    int code = https.GET();
    if (code == HTTP_CODE_OK) {
        response = https.getString();
        // Firebase retorna strings com aspas — remove-as
        if (response.startsWith("\"") && response.endsWith("\""))
            response = response.substring(1, response.length() - 1);
        https.end();
        return true;
    }

    Serial.printf("[FBase] GET erro %d: %s\n", code, https.getString().c_str());
    https.end();
    return false;
}

// ── HTTP PUT ──────────────────────────────────────────────────────────────────
bool FBase::httpPut(const String& url, const String& body, String& response)
{
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    HTTPClient https;
    if (!https.begin(client, url)) return false;

    https.addHeader("Content-Type", "application/json");
    int code = https.PUT(body);
    if (code == HTTP_CODE_OK || code == HTTP_CODE_NO_CONTENT) {
        response = https.getString();
        https.end();
        return true;
    }

    Serial.printf("[FBase] PUT erro %d\n", code);
    https.end();
    return false;
}

// ── Autenticação ──────────────────────────────────────────────────────────────
bool FBase::authenticate()
{
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);

    HTTPClient https;
    String url = String(AUTH_URL) + _apiKey;
    if (!https.begin(client, url)) return false;

    https.addHeader("Content-Type", "application/json");
    String body = "{\"email\":\"" + _email +
                  "\",\"password\":\"" + _password +
                  "\",\"returnSecureToken\":true}";

    int code = https.POST(body);
    if (code != HTTP_CODE_OK) {
        String err = https.getString();
        Serial.printf("[FBase] Auth erro %d: %s\n", code, err.c_str());
        if (err.indexOf("INVALID_PASSWORD")          != -1 ||
            err.indexOf("EMAIL_NOT_FOUND")           != -1 ||
            err.indexOf("INVALID_LOGIN_CREDENTIALS") != -1)
            _credentialError = true;
        https.end();
        return false;
    }

    String payload = https.getString();
    https.end();

    Serial.printf("[FBase] Auth payload (%d bytes): %.200s\n", payload.length(), payload.c_str());

    if (payload.length() < 10) {
        Serial.printf("[FBase] Auth: payload muito curto (%d bytes)\n", payload.length());
        return false;
    }

    // Parse JSON — aceita tanto "key":"value" quanto "key": "value" (com espaço)
    auto extract = [&](const String& key) -> String {
        String search = "\"" + key + "\"";
        int s = payload.indexOf(search);
        if (s == -1) return "";
        s += search.length();
        // pula espaços e o ':'
        while (s < (int)payload.length() && (payload[s] == ' ' || payload[s] == ':')) s++;
        // pula aspas de abertura
        if (s < (int)payload.length() && payload[s] == '"') s++;
        int e = payload.indexOf("\"", s);
        if (e == -1) return "";
        return payload.substring(s, e);
    };
    auto extractNum = [&](const String& key) -> long {
        String search = "\"" + key + "\"";
        int s = payload.indexOf(search);
        if (s == -1) return 0;
        s += search.length();
        while (s < (int)payload.length() && (payload[s] == ' ' || payload[s] == ':')) s++;
        int e = s;
        while (e < (int)payload.length() && payload[e] != ',' && payload[e] != '}' && payload[e] != '\n') e++;
        String val = payload.substring(s, e);
        val.trim();
        val.replace("\"", "");
        return val.toInt();
    };

    _idToken      = extract("idToken");
    _refreshToken = extract("refreshToken");
    long expiresIn = extractNum("expiresIn");
    _tokenExpiry  = millis() + (expiresIn - 60) * 1000UL;  // 1 min de margem

    if (_idToken.isEmpty()) {
        Serial.println("[FBase] Auth: idToken vazio.");
        return false;
    }

    Serial.println("[FBase] Autenticado com sucesso.");
    _ready = true;
    return true;
}

// ── Renovação de token ────────────────────────────────────────────────────────
bool FBase::refreshToken()
{
    if (_refreshToken.isEmpty()) return authenticate();

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000);

    HTTPClient https;
    String url = String(REFRESH_URL) + _apiKey;
    if (!https.begin(client, url)) return false;

    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=refresh_token&refresh_token=" + _refreshToken;

    int code = https.POST(body);
    if (code != HTTP_CODE_OK) {
        Serial.printf("[FBase] Refresh erro %d, reautenticando.\n", code);
        https.end();
        return authenticate();
    }

    String payload = https.getString();
    https.end();

    auto extract = [&](const String& key) -> String {
        String search = "\"" + key + "\"";
        int s = payload.indexOf(search);
        if (s == -1) return "";
        s += search.length();
        while (s < (int)payload.length() && (payload[s] == ' ' || payload[s] == ':')) s++;
        if (s < (int)payload.length() && payload[s] == '"') s++;
        int e = payload.indexOf("\"", s);
        if (e == -1) return "";
        return payload.substring(s, e);
    };

    String newToken = extract("id_token");
    String newRefresh = extract("refresh_token");
    if (newToken.isEmpty()) {
        Serial.println("[FBase] Refresh: token vazio, reautenticando.");
        return authenticate();
    }

    _idToken      = newToken;
    if (!newRefresh.isEmpty()) _refreshToken = newRefresh;
    _tokenExpiry  = millis() + (3600 - 60) * 1000UL;

    Serial.println("[FBase] Token renovado.");
    return true;
}

// ── ensureToken — garante token válido antes de qualquer operação ─────────────
bool FBase::ensureToken()
{
    if (WiFi.status() != WL_CONNECTED) return false;
    if (_idToken.isEmpty())              return authenticate();
    if (millis() >= _tokenExpiry)        return refreshToken();
    return true;
}

// ── init ──────────────────────────────────────────────────────────────────────
bool FBase::init()
{
    _ready = false;
    _credentialError = false;
    return authenticate();
}

bool FBase::stopApp()  { _ready = false; _idToken = ""; return true; }
void FBase::safeStop() { stopApp(); }
bool FBase::isReady()  { return _ready && WiFi.status() == WL_CONNECTED; }
bool FBase::isHealthy(){ return isReady(); }
bool FBase::isBusy()   { return false; }
bool FBase::hasCredentialError() { return _credentialError; }

// ── loop — renova token automaticamente ──────────────────────────────────────
void FBase::loop()
{
    if (!_ready || WiFi.status() != WL_CONNECTED) return;
    if (millis() >= _tokenExpiry) {
        Serial.println("[FBase] Token expirado, renovando...");
        refreshToken();
    }
}

// ── dbGetRaw — retorna JSON bruto (sem remover aspas) ────────────────────────
bool FBase::dbGetRaw(const String& path, String& result)
{
    if (!ensureToken()) return false;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);

    HTTPClient https;
    String url = buildUrl(path);
    if (!https.begin(client, url)) return false;

    int code = https.GET();
    if (code == HTTP_CODE_OK) {
        result = https.getString();
        https.end();
        return true;
    }

    Serial.printf("[FBase] GET raw erro %d\n", code);
    https.end();
    return false;
}

// ── dbGet ─────────────────────────────────────────────────────────────────────
bool FBase::dbGet(const String& path, String& result)
{
    if (!ensureToken()) return false;
    return httpGet(buildUrl(path), result);
}

// ── dbSet ─────────────────────────────────────────────────────────────────────
bool FBase::dbSet(const String& path, const String& value)
{
    if (!ensureToken()) return false;
    String resp;
    // String deve ser enviada com aspas no JSON
    return httpPut(buildUrl(path), "\"" + value + "\"", resp);
}

bool FBase::dbSet(const String& path, float value)
{
    if (!ensureToken()) return false;
    String resp;
    return httpPut(buildUrl(path), String(value, 4), resp);
}

bool FBase::dbSet(const String& path, bool value)
{
    if (!ensureToken()) return false;
    String resp;
    return httpPut(buildUrl(path), value ? "true" : "false", resp);
}

// ── Compatibilidade — awaitGet ────────────────────────────────────────────────
void FBase::awaitGet(String& path, String* result)
{
    if (!result) return;
    dbGet(path, *result);
}

void FBase::awaitGet(const char* path, char* result, unsigned int resultSize)
{
    if (!path || !result || resultSize == 0) return;
    String pathStr(path);
    String res;
    if (dbGet(pathStr, res)) {
        strncpy(result, res.c_str(), resultSize - 1);
        result[resultSize - 1] = '\0';
    }
}

// ── Compatibilidade — aSyncSet* (agora síncronos — REST é sempre síncrono) ───
void FBase::aSyncSetString(String path, String value)  { dbSet(path, value); }
void FBase::aSyncSetFloat(String path, float value)    { dbSet(path, value); }
void FBase::aSyncSetBool(String path, bool value)      { dbSet(path, value); }

void FBase::aSyncSetString(const char* p, const char* v) { if (p&&v) dbSet(String(p), String(v)); }
void FBase::aSyncSetFloat(const char* p, float v)        { if (p)    dbSet(String(p), v); }
void FBase::aSyncSetBool(const char* p, bool v)          { if (p)    dbSet(String(p), v); }

void FBase::aSyncGet(String& path, String& result)     { dbGet(path, result); }

void FBase::awaitSet(String& path, String value, int type)
{
    if      (type == FLOAT)  dbSet(path, value.toFloat());
    else if (type == BOOL)   dbSet(path, (bool)value.toInt());
    else                     dbSet(path, value);
}