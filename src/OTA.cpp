#include "OTA.hpp"

OTA::OTA(Preferences *prefs, FBase *firebase, Display *display) : prefs(prefs), firebase(firebase), display(display)
{
    binaryUrl = "";
    token     = "";
    version   = "";
    totalSize = 0;
    currentSize = 0;
    hasUpdate = false;
    isUpdated = false;
}

bool OTA::checkForUpdates()
{
    static String res  = "";
    static String path = "/_HasUpdate";
    firebase->aSyncGet(path, res);
    if (res == "false" || isUpdated == true) {
        hasUpdate = false;
        return false;
    }
    hasUpdate = true;
    return true;
}

void OTA::injectFbase(FBase *firebase)   { this->firebase = firebase; }
void OTA::injectDisplay(Display *display){ this->display  = display;  }

void OTA::setBinaryPath(String path) { binaryUrl = path; }
void OTA::setToken(String token)     { this->token = token; }
void OTA::setHasUpdate(bool status)  { hasUpdate = status; }
bool OTA::getHasUpdate()             { return hasUpdate; }

void OTA::setVersion(String new_version)
{
    prefs->begin("ota", false);
    prefs->putString("version", new_version);
    prefs->end();
}

// ── fetchReleaseInfo ──────────────────────────────────────────────────────────
// NÃO toca no Firebase — usa cliente SSL próprio e independente.
// WiFiClientSecure e HTTPClient alocados no HEAP (new/delete) para evitar
// stack leak de ~400 bytes por chamada que causava crash após ~22h.
int OTA::fetchReleaseInfo()
{
    if (binaryUrl.isEmpty()) return -1;

    WiFiClientSecure *client = new WiFiClientSecure();
    if (!client) return -1;
    client->setInsecure();
    client->setTimeout(15000);

    HTTPClient *https = new HTTPClient();
    if (!https) { delete client; return -1; }

    if (!https->begin(*client, binaryUrl)) {
        delete https;
        delete client;
        return -1;
    }

    https->addHeader("Authorization", String("token ") + token);
    https->addHeader("Accept",        "application/vnd.github.v3+json");
    https->addHeader("User-Agent",    "ESP32");

    int httpCode = https->GET();
    if (httpCode != HTTP_CODE_OK) {
        https->end();
        delete https;
        delete client;
        return -1;
    }

    String payload = https->getString();
    https->end();
    delete https;
    delete client;

    // ── TAG/VERSÃO ────────────────────────────────────────────────────
    int tagPos = payload.indexOf("\"tag_name\":\"");
    if (tagPos != -1) {
        tagPos += 12;
        releaseTag = payload.substring(tagPos, payload.indexOf("\"", tagPos));
    }

    // ── NOME ──────────────────────────────────────────────────────────
    int namePos = payload.indexOf("\"name\":\"");
    if (namePos != -1) {
        namePos += 8;
        releaseName = payload.substring(namePos, payload.indexOf("\"", namePos));
    }

    // ── DESCRIÇÃO ─────────────────────────────────────────────────────
    int bodyPos = payload.indexOf("\"body\":\"");
    if (bodyPos != -1) {
        bodyPos += 8;
        releaseBody = payload.substring(bodyPos, payload.indexOf("\"", bodyPos));
        releaseBody.replace("\\r\\n", "\n");
        releaseBody.replace("\\n",    "\n");
    }

    // ── DATA ──────────────────────────────────────────────────────────
    int datePos = payload.indexOf("\"published_at\":\"");
    if (datePos != -1) {
        datePos += 16;
        releaseDate = payload.substring(datePos, payload.indexOf("\"", datePos));
    }

    return 0;
}

// ── updateDevice ──────────────────────────────────────────────────────────────
// Para o Firebase apenas durante o download do binário — operação exclusiva.
// Reinicia o Firebase após conclusão (sucesso ou falha).
int OTA::updateDevice()
{
    display->logoScreen("Iniciando OTA...");

    if (!firebase || !firebase->stopApp()) return -1;

    const int barraTotal = 20;
    String    assetId       = "";
    String    tagParaSalvar = "";

    // BLOCO 1 — Busca do JSON
    {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(15000);
        HTTPClient https;

        if (https.begin(client, binaryUrl)) {
            https.addHeader("Authorization", String("token ") + token);
            https.addHeader("User-Agent",    "ESP32-OTA");

            if (https.GET() == HTTP_CODE_OK) {
                String payload = https.getString();

                int tagPos = payload.indexOf("\"tag_name\":\"");
                if (tagPos != -1) {
                    tagPos += 12;
                    tagParaSalvar = payload.substring(tagPos, payload.indexOf("\"", tagPos));
                }

                int assetsPos = payload.indexOf("\"assets\":[");
                if (assetsPos != -1) {
                    int idPos = payload.indexOf("\"id\":", assetsPos);
                    if (idPos != -1) {
                        idPos += 5;
                        assetId = payload.substring(idPos, payload.indexOf(",", idPos));
                        assetId.trim();
                    }
                }
            }
            https.end();
        }
    }

    if (assetId.isEmpty() || assetId == "null") {
        firebase->init();
        return -1;
    }

    // BLOCO 2 — Download do binário
    {
        WiFiClientSecure client2;
        client2.setInsecure();
        client2.setTimeout(30000);
        HTTPClient https2;

        String assetUrl = "https://api.github.com/repos/sobreiracaio/GrowESP2.0/releases/assets/" + assetId;
        https2.begin(client2, assetUrl);
        https2.addHeader("Authorization", String("token ") + token);
        https2.addHeader("Accept",        "application/octet-stream");
        https2.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int    httpCode   = https2.GET();
        size_t totalBytes = https2.getSize();

        if (httpCode == HTTP_CODE_OK && totalBytes > 0) {
            if (Update.begin(totalBytes)) {
                WiFiClient* stream     = https2.getStreamPtr();
                uint8_t     buffer[2048];
                size_t      downloaded = 0;
                int         lastBarra  = -1;

                while (https2.connected() && downloaded < totalBytes) {
                    esp_task_wdt_reset();
                    yield();

                    size_t avail = stream->available();
                    if (avail > 0) {
                        int bytesRead = stream->readBytes(buffer, min(avail, sizeof(buffer)));
                        if (bytesRead > 0) {
                            Update.write(buffer, bytesRead);
                            downloaded += bytesRead;

                            int barraAtual = (downloaded * barraTotal) / totalBytes;
                            if (barraAtual != lastBarra) {
                                // Barra fixa: sempre 20 blocos + percentual alinhado
                                // Usa caractere de bloco solido e traco para tamanho constante
                                String barra = "";
                                for (int i = 0; i < barraTotal; i++)
                                    barra += (i < barraAtual) ? "#" : "-";
                                int pct = (downloaded * 100) / totalBytes;
                                // Percentual sempre 3 digitos para nao mudar posicao
                                char pctBuf[8];
                                snprintf(pctBuf, sizeof(pctBuf), " %3d%%", pct);
                                barra += pctBuf;
                                display->logoScreen(barra);
                                lastBarra = barraAtual;
                            }
                        }
                    } else {
                        delay(10);
                    }
                }

                if (Update.end(true) && Update.isFinished()) {
                    if (tagParaSalvar.length() > 0) setVersion(tagParaSalvar);
                    display->logoScreen("Sucesso! Reiniciando");
                    delay(1500);
                    ESP.restart();
                    return 0;
                }
            }
        }
        https2.end();
    }

    display->logoScreen("Erro no Update!");
    delay(2000);
    firebase->init();
    return -1;
}

const String& OTA::getVersion()
{
    prefs->begin("ota", true);
    version = prefs->getString("version", version);
    prefs->end();
    return version;
}

const String& OTA::getReleaseTag()  { return releaseTag;  }
const String& OTA::getReleaseName() { return releaseName; }
const String& OTA::getReleaseBody() { return releaseBody; }

String OTA::getReleaseDate()
{
    if (releaseDate.length() < 16) return releaseDate;
    char buf[18];
    snprintf(buf, sizeof(buf), "%c%c/%c%c/%c%c%c%c %c%c:%c%c",
        releaseDate[8],  releaseDate[9],
        releaseDate[5],  releaseDate[6],
        releaseDate[0],  releaseDate[1],  releaseDate[2],  releaseDate[3],
        releaseDate[11], releaseDate[12],
        releaseDate[14], releaseDate[15]);
    return String(buf);
}