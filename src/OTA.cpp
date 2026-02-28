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
    // 1. Liberação de RAM Prévia
    if (!firebase || !firebase->stopApp()) {
        return -1;
    }
    
    display->logoScreen("Buscando Update...");
    String assetId = "";
    String tagParaSalvar = "";

    // BLOCO 1: Busca o ID do Asset (GitHub API)
    // As chaves { } garantem que os objetos SSL e HTTP saiam da RAM ao final do bloco
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient https;
        https.setTimeout(15000); // 15 segundos de timeout

        if (https.begin(client, binaryUrl)) {
            https.addHeader("Authorization", String("token ") + token);
            https.addHeader("User-Agent", "ESP32-OTA");

            int httpCode = https.GET();
            if (httpCode == HTTP_CODE_OK) {
                // Se o JSON for muito grande, o getString() pode causar crash. 
                // Idealmente usaríamos ArduinoJson, mas vamos limpar a String logo após o uso.
                String payload = https.getString();
                
                // Extração da TAG
                int tagPos = payload.indexOf("\"tag_name\":\"");
                if (tagPos != -1) {
                    tagPos += 12;
                    tagParaSalvar = payload.substring(tagPos, payload.indexOf("\"", tagPos));
                }

                // Extração do ID do Asset
                int assetsPos = payload.indexOf("\"assets\":[");
                if (assetsPos != -1) {
                    int idPos = payload.indexOf("\"id\":", assetsPos);
                    if (idPos != -1) {
                        idPos += 5;
                        assetId = payload.substring(idPos, payload.indexOf(",", idPos));
                        assetId.trim();
                    }
                }
                payload = ""; // Limpa a string gigante imediatamente
            }
            https.end();
        }
    } 

    // Se não achou o ID, volta pro Firebase
    if (assetId == "" || assetId == "null") {
        Serial.println("[OTA] Asset ID não encontrado.");
        firebase->init();
        return -1;
    }

    // Intervalo para o sistema respirar e reorganizar o Heap
    delay(500);
    yield();

    // BLOCO 2: Download e Escrita do Binário
    display->logoScreen("Baixando... 0%");
    int result = -1;

    {
        WiFiClientSecure client2;
        client2.setInsecure();
        // Opcional: client2.setBufferSizes(1024, 1024); // Economiza RAM se o servidor GitHub aceitar

        HTTPClient https2;
        String assetUrl = "https://api.github.com/repos/sobreiracaio/GrowESP2.0/releases/assets/" + assetId;
        
        https2.begin(client2, assetUrl);
        https2.addHeader("Authorization", String("token ") + token);
        https2.addHeader("Accept", "application/octet-stream");
        https2.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int httpCode = https2.GET();
        int totalSize = https2.getSize();

        if (httpCode == HTTP_CODE_OK && totalSize > 0) {
            if (Update.begin(totalSize)) {
                WiFiClient* stream = https2.getStreamPtr();
                uint8_t buffer[2048]; // Buffer de 2KB
                size_t downloaded = 0;
                int lastProgress = -1;

                while (https2.connected() && downloaded < totalSize) {
                    // Prevenção de Watchdog (WDT)
                    esp_task_wdt_reset(); 
                    yield();

                    size_t available = stream->available();
                    if (available > 0) {
                        int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));
                        if (bytesRead > 0) {
                            Update.write(buffer, bytesRead);
                            downloaded += bytesRead;

                            // Atualiza progresso no display (apenas se mudar o %)
                            int progress = (downloaded * 100) / totalSize;
                            if (progress % 5 == 0 && progress != lastProgress) {
                                String msg = "Baixando... " + String(progress) + "%";
                                display->logoScreen(msg.c_str());
                                lastProgress = progress;
                                Serial.printf("[OTA] Progresso: %d%%\n", progress);
                            }
                        }
                    } else {
                        delay(10); // Aguarda dados sem travar a CPU
                    }
                }

                if (Update.end(true)) {
                    if (Update.isFinished()) {
                        Serial.println("[OTA] Sucesso!");
                        if (tagParaSalvar.length() > 0) setVersion(tagParaSalvar);
                        result = 0; 
                    }
                } else {
                    Serial.printf("[OTA] Erro Update: %s\n", Update.errorString());
                }
            }
        }
        https2.end();
    }

    if (result == 0) {
        display->logoScreen("Sucesso! Reiniciando...");
        delay(2000);
        ESP.restart();
    } else {
        display->logoScreen("Erro na Atualização!");
        delay(2000);
        firebase->init();
    }

    return result;
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