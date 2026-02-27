#include "OTA.hpp"

OTA::OTA(Preferences *prefs, FBase *firebase, Display *display) : prefs(prefs), firebase(firebase), display(display)
{
    binaryUrl = "";
    token = "";
   
    version = "";

    totalSize = 0;
    currentSize = 0;
    hasUpdate = false;
    isUpdated = false;
    
}

bool OTA::checkForUpdates()
{
    static String res = "";
    static String path = "/_HasUpdate";
    firebase->aSyncGet(path, res);
    if(res == "false" || isUpdated == true)
    {
        hasUpdate = false;
        return false;
    }

    hasUpdate = true;
    return true;
    // if(isUpdated == false)
    // {
        
    // }
}

void OTA::injectFbase(FBase *firebase)
{
   this->firebase = firebase;
}

void OTA::injectDisplay(Display *display)
{
   this->display = display;
}

void OTA::setBinaryPath(String path)
{
    binaryUrl = path;
    
}
void OTA::setToken(String token)
{   
    this->token = token;
}

void OTA::setVersion(String new_version)
{
    prefs->begin("ota", false);
    prefs->putString("version", new_version);
    prefs->end();
}



void OTA::setHasUpdate(bool status)
{
    hasUpdate = status;
}

bool OTA::getHasUpdate()
{
    return hasUpdate;
}


int OTA::updateDevice()
{
    if (!firebase || !firebase->stopApp()) {
        return -1;
    }

    display->logoScreen("Atualizando, aguarde!");
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    if (!https.begin(client, binaryUrl))
    {
        firebase->init();
        return -1;
    }

    https.addHeader("Authorization", String("token ") + token);
    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.addHeader("User-Agent", "ESP32");

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        https.end();
        firebase->init();
        return -1;
    }

    String payload = https.getString();
    https.end();

    int tagPos = payload.indexOf("\"tag_name\":\"");
    if (tagPos != -1)
    {
        tagPos += 12;
        releaseTag = payload.substring(tagPos, payload.indexOf("\"", tagPos));
    }

    int namePos = payload.indexOf("\"name\":\"");
    if (namePos != -1)
    {
        namePos += 8;
        releaseName = payload.substring(namePos, payload.indexOf("\"", namePos));
    }

    int bodyPos = payload.indexOf("\"body\":\"");
    if (bodyPos != -1)
    {
        bodyPos += 8;
        releaseBody = payload.substring(bodyPos, payload.indexOf("\"", bodyPos));
        releaseBody.replace("\\r\\n", "\n");
        releaseBody.replace("\\n", "\n");
    }

    int datePos = payload.indexOf("\"published_at\":\"");
    if (datePos != -1)
    {
        datePos += 16;
        releaseDate = payload.substring(datePos, payload.indexOf("\"", datePos));
    }

    int assetsPos = payload.indexOf("\"assets\":[");
    if (assetsPos == -1)
    {
        firebase->init();
        return -1;
    }

    int idPos = payload.indexOf("\"id\":", assetsPos);
    if (idPos == -1)
    {
        firebase->init();
        return -1;
    }

    idPos += 5;
    String assetId = payload.substring(idPos, payload.indexOf(",", idPos));
    assetId.trim();
    payload = ""; // libera heap antes do download

    String assetUrl = "https://api.github.com/repos/sobreiracaio/GrowESP2.0/releases/assets/" + assetId;

    WiFiClientSecure client2;
    client2.setInsecure();

    HTTPClient https2;
    if (!https2.begin(client2, assetUrl))
    {
        firebase->init();
        return -1;
    }

    https2.addHeader("Authorization", String("token ") + token);
    https2.addHeader("Accept", "application/octet-stream");
    https2.addHeader("User-Agent", "ESP32");
    https2.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    httpCode = https2.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        https2.end();
        firebase->init();
        return -1;
    }

    totalSize   = https2.getSize();
    currentSize = 0;

    if (totalSize <= 0)
    {
        https2.end();
        firebase->init();
        return -1;
    }

    if (!Update.begin(totalSize))
    {
        https2.end();
        firebase->init();
        return -1;
    }

    WiFiClient* stream = https2.getStreamPtr();
    uint8_t     buffer[1024];

    while (https2.connected() && currentSize < totalSize)
    {
        esp_task_wdt_reset();
        size_t available = stream->available();
        if (available)
        {
            int bytesRead = stream->readBytes(
                buffer,
                (available > sizeof(buffer)) ? sizeof(buffer) : available
            );
            if (bytesRead > 0)
            {
                if (Update.write(buffer, bytesRead) != bytesRead)
                {
                    https2.end();
                    firebase->init();
                    return -1;
                }
                currentSize += bytesRead;
            }
        }
        delay(1);
    }

    https2.end();

    if (!Update.end(true) || !Update.isFinished())
    {
        firebase->init();
        return -1;
    }

    // Grava versão no Preferences — sem Firebase, sem SSL, sem competição de heap
    if (releaseTag.length() > 0)
    {
        setVersion(releaseTag);
        Serial.printf("[OTA] Versao %s salva em Preferences\n", releaseTag.c_str());
    }

    display->logoScreen("Sistema Atualizado com Sucesso! Reiniciando!");
    delay(500);
    ESP.restart();
    return 0;
}

int OTA::fetchReleaseInfo()
{
    firebase->stopApp();
    WiFiClientSecure client;
    client.setInsecure();
    Serial.printf("[OTA] URL: %s\n", binaryUrl.c_str());
    Serial.printf("[OTA] Token: %s\n", token.c_str());
    HTTPClient https;
    if (!https.begin(client, binaryUrl))
    {
        Serial.println("[OTA] Falha ao iniciar conexao");
        firebase->init();
        return -1;
    }

    https.addHeader("Authorization", String("token ") + token);
    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.addHeader("User-Agent", "ESP32");

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("[OTA] HTTP erro ao buscar release: %d\n", httpCode);
        https.end();
        firebase->init();
        return -1;
    }

    String payload = https.getString();
    https.end();

    // ========== TAG/VERSÃO ==========
    int tagPos = payload.indexOf("\"tag_name\":\"");
    if (tagPos != -1)
    {
        tagPos += 12;
        int tagEnd = payload.indexOf("\"", tagPos);
        releaseTag = payload.substring(tagPos, tagEnd);
    }

    // ========== NOME DA RELEASE ==========
    int namePos = payload.indexOf("\"name\":\"");
    if (namePos != -1)
    {
        namePos += 8;
        int nameEnd = payload.indexOf("\"", namePos);
        releaseName = payload.substring(namePos, nameEnd);
    }

    // ========== DESCRIÇÃO ==========
    int bodyPos = payload.indexOf("\"body\":\"");
    if (bodyPos != -1)
    {
        bodyPos += 8;
        int bodyEnd = payload.indexOf("\"", bodyPos);
        releaseBody = payload.substring(bodyPos, bodyEnd);
        releaseBody.replace("\\r\\n", "\n");
        releaseBody.replace("\\n", "\n");
    }

    // ========== DATA DE PUBLICAÇÃO ==========
    int datePos = payload.indexOf("\"published_at\":\"");
    if (datePos != -1)
    {
        datePos += 16;
        int dateEnd = payload.indexOf("\"", datePos);
        releaseDate = payload.substring(datePos, dateEnd);
    }

    Serial.printf("[OTA] Tag: %s\n", releaseTag.c_str());
    Serial.printf("[OTA] Nome: %s\n", releaseName.c_str());
    Serial.printf("[OTA] Data: %s\n", releaseDate.c_str());
    Serial.printf("[OTA] Descricao: %s\n", releaseBody.c_str());
    firebase->init();
    return 0;
}

const String& OTA::getVersion()
{
    prefs->begin("ota", true);
    version = prefs->getString("version", version);
    prefs->end();
    return version; 
}
const String& OTA::getReleaseTag()  { return releaseTag; }
const String& OTA::getReleaseName() { return releaseName; }
const String& OTA::getReleaseBody() { return releaseBody; }

String OTA::getReleaseDate()
{
    if (releaseDate.length() < 16) return releaseDate;
    char buf[18];
    snprintf(buf, sizeof(buf), "%c%c/%c%c/%c%c%c%c %c%c:%c%c",
        releaseDate[8],  releaseDate[9],   // dia
        releaseDate[5],  releaseDate[6],   // mes
        releaseDate[0],  releaseDate[1],   releaseDate[2],  releaseDate[3], // ano
        releaseDate[11], releaseDate[12],  // hora
        releaseDate[14], releaseDate[15]); // minuto
    return String(buf);
}