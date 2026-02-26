#include "Serial.hpp"


void sendPacket(uint8_t id, float value)
{
    static uint8_t lastID = 0xFF;
    static float lastValue = -9999;
    static unsigned long lastSendTime = 0;
    
    // ⭐ ESPERA no mínimo 50ms entre QUALQUER pacote
    if(millis() - lastSendTime < 50)
        return;
    
    
    // Evita duplicados
    if(id == lastID && value == lastValue) 
    {
        return;
    }
    
    lastID = id;
    lastValue = value;
    lastSendTime = millis();
    
    uint32_t lvalue = (uint32_t)(value * 100);
    
    uint8_t byte3 = (lvalue >> 24) & 0xFF;
    uint8_t byte2 = (lvalue >> 16) & 0xFF;
    uint8_t byte1 = (lvalue >> 8) & 0xFF;
    uint8_t byte0 = lvalue & 0xFF;
    
    uint8_t checksum = (HEADER + id + byte3 + byte2 + byte1 + byte0) & 0xFF;

    // Serial.printf("📤 TX ID:0x%02X Val:%.2f Raw:0x%02X%02X%02X%02X Chk:0x%02X\n", 
    //                id, value, byte3, byte2, byte1, byte0, checksum);

    Serial2.write(HEADER);
    Serial2.write(id);
    Serial2.write(byte3);
    Serial2.write(byte2);
    Serial2.write(byte1);
    Serial2.write(byte0);
    Serial2.write(checksum);
    
    Serial2.flush(); // Garante que enviou
}


int readPacket(DataClass *data_class)
{
    if (Serial2.available() < 5)
        return 0;

    uint8_t header = Serial2.read();
    
    //✅ DEBUG: Veja o que está chegando
    //Serial.print("📥 Recebido do Pico - Header:0x");
    //Serial.print(header, HEX);
    //Serial.print(" (esperado:0x");
    //Serial.print(HEADER, HEX);
    //Serial.println(")");
    
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

     //Serial.printf("Recebido ID:0x%02X High:0x%02X Low:0x%02X Chk:0x%02X\n", 
                    //id, high, low, checksum);

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
            data_class->setSoil(fvalue * 10);
            //Serial.printf("   -> SOIL definido: %.2f\n", fvalue * 10);
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
            //Serial.printf("❌ ID desconhecido: 0x%02X\n", id);
            return -3;
    }

    return 1;
}