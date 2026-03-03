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
    // Feedback imediato: trava a percepção do usuário no menu de "Aguarde"
    display->logoScreen("Iniciando OTA...");
    
    if (!firebase || !firebase->stopApp()) {
        return -1;
    }

    String assetId = "";
    String tagParaSalvar = "";
    const int barraTotal = 20; // Quantidade de caracteres "|" na barra cheia

    // BLOCO 1: Busca do JSON (Escopo isolado para RAM)
    {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient https;
        https.setTimeout(15000);

        if (https.begin(client, binaryUrl)) {
            https.addHeader("Authorization", String("token ") + token);
            https.addHeader("User-Agent", "ESP32-OTA");

            int httpCode = https.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = https.getString();
                
                // Extração simples de Tag e Asset ID
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

    if (assetId == "" || assetId == "null") {
        firebase->init();
        return -1;
    }

    // BLOCO 2: Download do Binário com Barra de Progresso
    {
        WiFiClientSecure client2;
        client2.setInsecure();
        HTTPClient https2;
        
        String assetUrl = "https://api.github.com/repos/sobreiracaio/GrowESP2.0/releases/assets/" + assetId;
        https2.begin(client2, assetUrl);
        https2.addHeader("Authorization", String("token ") + token);
        https2.addHeader("Accept", "application/octet-stream");
        https2.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        int httpCode = https2.GET();
        size_t totalSize = https2.getSize();

        if (httpCode == HTTP_CODE_OK && totalSize > 0) {
            if (Update.begin(totalSize)) {
                WiFiClient* stream = https2.getStreamPtr();
                uint8_t buffer[2048];
                size_t downloaded = 0;
                int lastBarraCount = -1;

                while (https2.connected() && downloaded < totalSize) {
                    esp_task_wdt_reset();
                    yield();

                    size_t available = stream->available();
                    if (available > 0) {
                        int bytesRead = stream->readBytes(buffer, min(available, sizeof(buffer)));
                        if (bytesRead > 0) {
                            Update.write(buffer, bytesRead);
                            downloaded += bytesRead;

                            // Cálculo da Barra de Progresso [||||      ]
                            int progressoPorcento = (downloaded * 100) / totalSize;
                            int barraAtual = (downloaded * barraTotal) / totalSize;

                            // Atualiza display apenas se a barra mudar (otimização de CPU)
                            if (barraAtual != lastBarraCount) {
                                String visualBarra = "[";
                                for (int i = 0; i < barraTotal; i++) {
                                    if (i < barraAtual) visualBarra += "|";
                                    else visualBarra += " ";
                                }
                                visualBarra += "] " + String(progressoPorcento) + "%";
                                
                                display->logoScreen(visualBarra.c_str());
                                lastBarraCount = barraAtual;
                            }
                        }
                    } else {
                        delay(10);
                    }
                }

                if (Update.end(true)) {
                    if (Update.isFinished()) {
                        if (tagParaSalvar.length() > 0) setVersion(tagParaSalvar);
                        display->logoScreen("Sucesso! Reiniciando");
                        delay(1500);
                        ESP.restart();
                        return 0;
                    }
                }
            }
        }
        https2.end();
    }

    // Se chegar aqui, algo deu errado
    display->logoScreen("Erro no Update!");
    delay(2000);
    firebase->init();
    return -1;
}

int OTA::fetchReleaseInfo()
{
    firebase->stopApp();
    WiFiClientSecure client;
    client.setInsecure();
    //Serial.printf("[OTA] URL: %s\n", binaryUrl.c_str());
    //Serial.printf("[OTA] Token: %s\n", token.c_str());
    HTTPClient https;
    if (!https.begin(client, binaryUrl))
    {
        //Serial.println("[OTA] Falha ao iniciar conexao");
        firebase->init();
        return -1;
    }

    https.addHeader("Authorization", String("token ") + token);
    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.addHeader("User-Agent", "ESP32");

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        //Serial.printf("[OTA] HTTP erro ao buscar release: %d\n", httpCode);
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

    //Serial.printf("[OTA] Tag: %s\n", releaseTag.c_str());
    //Serial.printf("[OTA] Nome: %s\n", releaseName.c_str());
    //Serial.printf("[OTA] Data: %s\n", releaseDate.c_str());
    //Serial.printf("[OTA] Descricao: %s\n", releaseBody.c_str());
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