#include "OTA.hpp"

OTA::OTA(Preferences *prefs, FBase *firebase, Display *display) : prefs(prefs), firebase(firebase), display(display)
{
    binaryUrl = "";
    token = "";
    serialNum = 0;
    digit1 = 0;
    digit2 = 0;
    digit3 = 0;

    version = "";
    
}

void OTA::setSerialNum()
{
    int res = (digit1 * 100) + (digit2 * 10) + digit3;
    serialNum = res;
}

int OTA::getSerialNum()
{   
    return serialNum;
}

void OTA::setDigit(int position, int value)
{
    switch (position)
    {
    case 1:
        digit1 = value;
        return;
    case 2:
        digit2 = value;
        return;
    case 3:
        digit3 = value;
        return;
    default:
        return;
    }
}

int OTA::getDigit(int position)
{
    switch (position)
    {
    case 1:
        return digit1;
    case 2:
        return digit2;
    case 3:
        return digit3;
    default:
        return -1;
    }
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
    version = new_version;
}

String OTA::getVersion()
{
    return version;
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
    isOTA = true;

    
    // Verificação do Firebase
    if(!firebase->stopApp())
    {
        isOTA = false;
        return -1;
    }

    WiFiClientSecure client;
    client.setInsecure();

    // ========== PASSO 1: Buscar informações da release ==========
    HTTPClient https;
    if (!https.begin(client, binaryUrl))
    {
        display->OTAScreen("Falha ao iniciar conexao. Tente novamente.");
        //Serial.println("[OTA] Falha ao iniciar conexao");
        isOTA = false;
        return -1;
    }

    https.addHeader("Authorization", String("token ") + token);
    https.addHeader("Accept", "application/vnd.github.v3+json");
    https.addHeader("User-Agent", "ESP32");

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        display->OTAScreen("HTTP erro ao buscar release: " + httpCode);
        //Serial.printf("[OTA] HTTP erro ao buscar release: %d\n", httpCode);
        https.end();
        isOTA = false;
        return -1;
    }

    // Ler o JSON da resposta
    String payload = https.getString();
    https.end();
    display->OTAScreen("JSON recebido, extraindo informações...");
    //Serial.println("[OTA] JSON recebido, extraindo informações...");

    // ========== EXTRAIR TAG/VERSÃO DA RELEASE ==========
    int tagPos = payload.indexOf("\"tag_name\":\"");
    String releaseTag = "";
    
    if (tagPos != -1)
    {
        tagPos += 12; // Pular "tag_name":"
        int tagEnd = payload.indexOf("\"", tagPos);
        releaseTag = payload.substring(tagPos, tagEnd);
        display->OTAScreen("Versao: " + releaseTag);
        //Serial.printf("[OTA] Tag da release: %s\n", releaseTag.c_str());
    }
    else
    {
         display->OTAScreen("Versao nao encontrada.");
        //Serial.println("[OTA] Tag não encontrada no JSON");
    }

    // ========== PASSO 2: Extrair o ASSET ID ==========
    int assetsPos = payload.indexOf("\"assets\":[");
    if (assetsPos == -1)
    {
        display->OTAScreen("Nenhum asset encontrado na release");
        //Serial.println("[OTA] Nenhum asset encontrado na release");
        isOTA = false;
        return -1;
    }

    // Procurar o primeiro "id" após "assets"
    int idPos = payload.indexOf("\"id\":", assetsPos);
    if (idPos == -1)
    {
        display->OTAScreen("ID do asset nao encontrado");
        //Serial.println("[OTA] ID do asset não encontrado");
        isOTA = false;
        return -1;
    }

    idPos += 5; // Pular "id":
    int idEnd = payload.indexOf(",", idPos);
    String assetId = payload.substring(idPos, idEnd);
    assetId.trim();

    display->OTAScreen("Asset ID encontrado: " + assetId);
    //Serial.printf("[OTA] Asset ID encontrado: %s\n", assetId.c_str());

    // Construir URL da API para download do asset
    String assetUrl = "https://api.github.com/repos/sobreiracaio/GrowESP2.0/releases/assets/" + assetId;

    // ========== PASSO 3: Baixar o binário via API ==========
    WiFiClientSecure client2;
    client2.setInsecure();

    HTTPClient https2;
    if (!https2.begin(client2, assetUrl))
    {
        display->OTAScreen("Falha ao conectar no asset");
        //Serial.println("[OTA] Falha ao conectar no asset");
        isOTA = false;
        return -1;
    }

    https2.addHeader("Authorization", String("token ") + token);
    https2.addHeader("Accept", "application/octet-stream");
    https2.addHeader("User-Agent", "ESP32");
    https2.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    display->OTAScreen("Iniciando download do binario...");
    //Serial.println("[OTA] Iniciando download do binário...");

    httpCode = https2.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        display->OTAScreen("HTTP erro ao baixar binario: " + httpCode);
        //Serial.printf("[OTA] HTTP erro ao baixar binário: %d\n", httpCode);
        https2.end();
        isOTA = false;
        return -1;
    }

    totalSize = https2.getSize();
    currentSize = 0;

    if (totalSize <= 0)
    {
        display->OTAScreen("Tamanho do binario invalido");
        //Serial.println("[OTA] Tamanho do binário inválido");
        https2.end();
        isOTA = false;
        return -1;
    }

    display->OTAScreen("Tamanho do binario: " + totalSize);
    //Serial.printf("[OTA] Tamanho do binário: %d bytes\n", totalSize);

    if (!Update.begin(totalSize))
    {
        display->OTAScreen("Falha ao iniciar atualizacao: " + Update.getError());
        //Serial.printf("[OTA] Falha ao iniciar Update: %d\n", Update.getError());
        https2.end();
        isOTA = false;
        return -1;
    }

    WiFiClient *stream = https2.getStreamPtr();
    uint8_t buffer[1024];

    while (https2.connected() && currentSize < totalSize)
    {
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
                    display->OTAScreen("Erro ao escrever no Update");
                    //Serial.println("[OTA] Erro ao escrever no Update");
                    https2.end();
                    isOTA = false;
                    return -1;
                }
                currentSize += bytesRead;
                
                if (currentSize % 10240 == 0)
                {
                    display->OTAScreen("Progresso...");
                    // Serial.printf("[OTA] Progresso: %d/%d bytes (%.1f%%)\n", 
                    //               currentSize, totalSize, 
                    //               (float)currentSize * 100.0 / totalSize);
                }
            }
        }
        delay(1);
    }

    https2.end();

    if (!Update.end(true))
    {
        display->OTAScreen("Erro ao finalizar atualizacao: " + Update.getError());
        //Serial.printf("[OTA] Erro ao finalizar Update: %d\n", Update.getError());
        isOTA = false;
        return -1;
    }

    if (!Update.isFinished())
    {
        display->OTAScreen("Atualizacao nao foi finalizada corretamente");
        //Serial.println("[OTA] Update não foi finalizado corretamente");
        isOTA = false;
        return -1;
    }
    display->OTAScreen("Atualizacao concluida com sucesso! Reiniciando...");
    //Serial.println("[OTA] Atualização concluída com sucesso!");
    
    // ========== SALVAR A TAG/VERSÃO ==========
    if (releaseTag.length() > 0)
    {
        //Serial.printf("[OTA] Salvando versão: %s\n", releaseTag.c_str());
        setVersion(releaseTag);
    }
    
    delay(500);
    ESP.restart();

    return 0;
}