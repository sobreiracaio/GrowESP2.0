#pragma once

#define HEADER 0xAA


//ID CODES FOR VALUES FOR SENDING ON SERIAL
#define TT 0x01
#define TTOL 0x02
#define TH 0x03
#define HTOL 0x04
#define TS 0x05
#define STOL 0x06
#define PD 0x07
#define AD 0x08
#define LIGHT0 0x09 //send
#define STATUS0 0x10
#define PUMP_CAL 0x11

//ID CODES FOR VALUES FOR RECEIVING ON SERIAL

#define TEMP 0x20
#define HUMID 0x21
#define SOIL 0x22
#define LIGHT 0x23 //receive
#define PUMP 0x24
#define COOLER 0x25
#define HEATER 0x26
#define HUMIDIFIER 0x27
#define DEHUMIDIFIER 0x28
#define RESERV 0x29


#include "Libraries.hpp"
#include "DataClass.hpp"


void sendPacket(uint8_t id, float value);
int readPacket(DataClass *data_class);