#include "Libraries.hpp"


#include "WifiManager.hpp"
#include "Time.hpp"
#include "FBase.hpp"
#include "Display.hpp"
#include "Button.hpp"
#include "DataClass.hpp"
#include "OTA.hpp"
#include "Light.hpp"

//Libraries

Preferences prefs;
UpdateClass update;


TFT_eSPI *tft = NULL;



//Classes
Light light;
WifiManager wifi(&prefs, nullptr);
Time rtc("pool.ntp.org", "pool.ntp.br", -10800, 0);
FBase *firebase = NULL;
Display *display = NULL;
Button* button[4] = {NULL};
DataClass data_class(&light);
OTA ota(&prefs);


struct tm now = {0};
String safeEmail = "";
String status = "";

float menu = 0;

void initClasses()
{
    tft = new TFT_eSPI();
	button[0] = new Button(BTN1);
	button[1] = new Button(BTN2);
	button[2] = new Button(BTN3);
	button[3] = new Button(BTN4);
	display = new Display(tft, firebase, &rtc, &ota, &wifi, button, &data_class); 
	wifi.injectDisplay(display);
}

void fireBaseLoadData();

void initModules()
{
	display->initDisplay();
	display->initLogoScreen();
	wifi.wifiInit();
    display->connectionScreen("Atualizando relogio", "     Aguarde...     ");
	rtc.begin();
	display->flushScreen();
	for (int i = 0; i < 4; i++)
		button[i]->init();
    
    safeEmail = wifi.getEmail();
    safeEmail.replace(".","_");
    firebase = new FBase(API_KEY, DATABASE_URL, wifi.getEmail(), wifi.getPass());
    display->injectFBase(firebase);
    display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
    while (!firebase->init())
        display->connectionScreen("Atualizando banco de dados", "     Aguarde...     ");
    firebase->awaitSet(safeEmail + "/Pass", wifi.getPass(), STRING);
    fireBaseLoadData();
	

}

void formatDate(String *date, int period)
{
     int colonIndex = date->indexOf(':');
    if (colonIndex == -1) return; // formato inválido
   
    float hour = date->substring(0, colonIndex).toFloat();
    float minute = date->substring(colonIndex + 1).toFloat();

  

    if(period == DAY)
    {
        data_class.setDayTime(hour, minute);
        
    }
    if(period == NIGHT)
    {
        data_class.setNightTime(hour, minute);
        
    }

}

void fireBaseLoadData()
{
    String dayTime = "";
    String nightTime = "";

    String targetTemp = "";
    String tempTol = "";

    String targetHumid = "";
    String humidTol = "";

    String targetSoil = "";
    String pumpDur = "";
    String absDelay = "";
    String soilTol = "";

    

    firebase->awaitGet(safeEmail + "/InsertedData/Light/HourOn", &dayTime);
    formatDate(&dayTime, DAY);

    firebase->awaitGet(safeEmail + "/InsertedData/Light/HourOff", &nightTime);
    formatDate(&nightTime, NIGHT);

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Temperature/TargetTemp", &targetTemp);
    data_class.setTargetTemp(targetTemp.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Temperature/TempTolerance", &tempTol);
    data_class.setTempTolerance(tempTol.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Humid/TargetHumid", &targetHumid);
    data_class.setTargetHumid(targetHumid.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Humid/HumidTolerance", &humidTol);
    data_class.setHumidTolerance(humidTol.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/TargetSoil", &targetSoil);
    data_class.setTargetSoil(targetSoil.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/PumpDuration", &pumpDur);
    data_class.setPumpDuration(pumpDur.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/AbsorptionDelay", &absDelay);
    data_class.setAbsorptionDelay(absDelay.toFloat());

    firebase->awaitGet(safeEmail + "/InsertedData/Sensor/Soil/SoilTolerance", &soilTol);
    data_class.setSoilTolerance(soilTol.toFloat());

    firebase->awaitGet(safeEmail + "/Status", &status);
    if (status == "true")
        data_class.setIsRunning(true);
    else
        data_class.setIsRunning(false);
}

void parseReceivedData(const char *receivedData)
{
    static char lastData[128] = "";

    if (strcmp(receivedData, lastData) == 0)
        return;

    // Cópia segura da nova string recebida
    strncpy(lastData, receivedData, sizeof(lastData) - 1);
    lastData[sizeof(lastData) - 1] = '\0';

    // Variáveis temporárias para cada sensor
    static float temp = 0.0f, humid = 0.0f, soil = 0.0f;
    bool light = 0, pump = 0, cooler = 0, heater = 0, humidifier = 0, dehumidifier = 0;

    // Formato esperado: "T:25.5 H:60.2 S:45.0 L:1 P:0 C:1 He:0 Hu:1 DHu:0"
    int parsed = sscanf(receivedData,
                        "T:%f H:%f S:%f L:%d P:%d C:%d He:%d Hu:%d DHu:%d",
                        &temp, &humid, &soil,
                        &light, &pump, &cooler, &heater, &humidifier, &dehumidifier);

    if (parsed >= 3)
    {
        data_class.setTemp(temp);
        data_class.setHumid(humid);
        data_class.setSoil(soil);

        if (parsed >= 4) data_class.setLightStatus(light);
        if (parsed >= 5) data_class.setPumpStatus(pump);
        if (parsed >= 6) data_class.setCoolerStatus(cooler);
        if (parsed >= 7) data_class.setHeaterStatus(heater);
        if (parsed >= 8) data_class.setHumidStatus(humidifier);
        if (parsed >= 9) data_class.setDehumidStatus(dehumidifier);
    }
}

//Serial.printf("L:%d P:%d C:%d He:%d Hu:%d DHu:%d\n", data_class.getLightStatus(), data_class.getPumpStatus(), data_class.getCoolerStatus(), data_class.getHeaterStatus(), data_class.getHumidStatus(), data_class.getDehumidStatus());

String parseDataToSend()
{
  String res;
  res  = "TT: "   + String(data_class.getTargetTemp()) ;
  res += " TTol: " + String(data_class.getTempTolerance()) ;
  res += " TH: "   + String(data_class.getTargetHumid()) ;
  res += " HTol: " + String(data_class.getHumidTolerance()) ;
  res += " TS: "   + String(data_class.getTargetSoil()) ;
  res += " STol: " + String(data_class.getSoilTolerance()) ;
  res += " PD: "   + String(data_class.getPumpDuration()) ;
  res += " AD: "   + String(data_class.getAbsorptionDelay()) ;
  res += " L: "    + String(data_class.getIsRunning() ? light.getStatus() : false);
  res += " STATUS: " + String(data_class.getIsRunning());
  res += "\n";

  return res;
}

String readData()
{
    if (Serial1.available()) 
    {
        // Retorna o objeto String. O chamador é responsável por gerenciar a memória
        return Serial1.readStringUntil('\n');
    }
    // Retorna String vazia, não o ponteiro vazio, o que é mais seguro.
    return "";
}

void sendData(String data)
{
    static String lastData = "";

	if(data.equals(lastData))
		return;

	Serial1.print(data);
    lastData = data;
}

void getNow()
{
  now.tm_hour = rtc.getHour();
  now.tm_min = rtc.getMinute();
  now.tm_sec = rtc.getSecond();
  now.tm_mday = rtc.getDay();
  now.tm_mon = rtc.getMonth();
  now.tm_year = rtc.getYear();
  rtc.checkSync();
}

void setup() 
{
	Serial.begin(115200);
	Serial1.begin(9600, SERIAL_8N1, UART_RX, UART_TX);
	initClasses();
	initModules();
    
	light.setTimeFunction(getNow);



}

String dataRead = "";

void loop() 
{
	display->menuSwitch(&menu);
    wifi.loop();
    firebase->loop();
	getNow();
    
    dataRead = readData();
   
    parseReceivedData(dataRead.c_str());
    Serial.println(dataRead);
    

	sendData(parseDataToSend());
	

   
    
	light.run(data_class.getIsRunning());
	
	
}