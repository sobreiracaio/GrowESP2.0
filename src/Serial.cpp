#include "Serial.hpp"


void sendPacket(uint8_t id, float value)
{
    // ✅ Evita envios duplicados muito rápidos
    static uint8_t lastID = 0xFF;
    static float lastValue = -9999;
    static unsigned long lastSendTime = 0;
    
    // Se for o mesmo pacote em menos de 100ms, ignora
    if(id == lastID && value == lastValue && millis())// - lastSendTime < 100) 
    {
        //Serial.printf("⏭️  Pulando envio duplicado ID:0x%02X\n", id);
        return;
    }
    
    lastID = id;
    lastValue = value;
    lastSendTime = millis();
    
    float multiply = value * 100;
    uint16_t fvalue = (uint16_t)multiply;

    
    uint8_t high = (fvalue >> 8) & 0xFF;
    uint8_t low  = fvalue & 0xFF;

    uint8_t checksum = (HEADER + id + high + low) & 0xFF;

    Serial.printf("📤 Enviando ID:0x%02X Val:%.2f Raw:0x%02X%02X Chk:0x%02X\n", 
                   id, value, high, low, checksum);

    Serial2.write(HEADER);
    Serial2.write(id);
    Serial2.write(high);
    Serial2.write(low);
    Serial2.write(checksum);
}


int readPacket(DataClass *data_class)
{
    if (Serial2.available() < 5)
        return 0;

    uint8_t header = Serial2.read();
    
    // ✅ DEBUG: Veja o que está chegando
    // Serial.print("📥 Recebido do Pico - Header:0x");
    // Serial.print(header, HEX);
    // Serial.print(" (esperado:0x");
    // Serial.print(HEADER, HEX);
    // Serial.println(")");
    
    if (header != HEADER) {
        //Serial.printf("❌ Header inválido: 0x%02X\n", header);
        // ✅ Limpa buffer até achar próximo header
        while(Serial2.available() && Serial2.peek() != HEADER) {
            Serial2.read();
        }
        return -1;
    }

    uint8_t id = (uint8_t)Serial2.read();
    uint8_t high = (uint8_t)Serial2.read();
    uint8_t low  = (uint8_t)Serial2.read();
    uint8_t checksum = (uint8_t)Serial2.read();

    // Serial.printf("Recebido ID:0x%02X High:0x%02X Low:0x%02X Chk:0x%02X\n", 
    //                id, high, low, checksum);

    uint8_t calc = (HEADER + id + high + low) & 0xFF;
    
    if (calc != checksum) {
        //Serial.printf("❌ Checksum inválido! Calc:0x%02X != Recv:0x%02X\n", calc, checksum);
        return -2;
    }

    uint16_t value = ((uint16_t)high << 8) | low;
    float fvalue = (float)value / 100.0;
    
    //Serial.printf("✅ Pacote OK! ID:0x%02X Valor:%.2f\n", id, fvalue);

    switch (id)
    {
        case TEMP:
            data_class->setTemp(fvalue);
            //Serial.printf("   -> TEMP definido: %.2f\n", fvalue);
            break;
        
        case HUMID:
            data_class->setHumid(fvalue);
            //Serial.printf("   -> HUMID definido: %.2f\n", fvalue);
            break;
        
        case SOIL:
            data_class->setSoil(fvalue);
            //Serial.printf("   -> SOIL definido: %.2f\n", fvalue);
            break;
        
        case LIGHT:
            data_class->setLightStatus((bool)fvalue);
            //Serial.printf("   -> LIGHT0 definido: %d\n", (bool)fvalue);
            break;
        
        case PUMP:
            data_class->setPumpStatus((bool)fvalue);
            //Serial.printf("   -> PUMP definido: %d\n", (bool)fvalue);
            break;
        
        case COOLER:
            data_class->setCoolerStatus((bool)fvalue);
            //Serial.printf("   -> COOLER definido: %d\n", (bool)fvalue);
            break;
        
        case HEATER:
            data_class->setHeaterStatus((bool)fvalue);
            //Serial.printf("   -> HEATER definido: %d\n", (bool)fvalue);
            break;
        
        case HUMIDIFIER:
            data_class->setHumidStatus((bool)fvalue);
            //Serial.printf("   -> HUMIDIFIER definido: %d\n", (bool)fvalue);
            break;
        
        case DEHUMIDIFIER:
            data_class->setDehumidStatus((bool)fvalue);
            //Serial.printf("   -> DEHUMIDIFIER definido: %d\n", (bool)fvalue);
            break;
        
        case RESERV:
            data_class->setWaterRes(fvalue);
            //Serial.printf("   -> RESERV definido: %.2f\n", fvalue);
            break;
        
        default:
           // Serial.printf("❌ ID desconhecido: 0x%02X\n", id);
            return -3;
    }

    return 1;
}