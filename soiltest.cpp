/*
  This code for a Soil Moisture sensor and a Moist Level Indicator has been developed and produced by Pierre Pennings (December 2018)
  This application can be used e.g. in automatic plant watering systems  
  The DIY Moisture Sensor uses 2 pieces of fondue forks
  The DIY Moist Level Indicator is made with 5 (Neopixel) SMD5050 LEDs with WS2812B controller chips powered with 5 V
  The Moist Level is measured once every second (during 5 seconds) and determines the average of 5 consecutive measurements in 5 different levels
  The Moist Level Indicator is set to the measured soil moisture level
  This sketch is written for using 1 Moisture sensor, but in the final Plantwatering project 3 different sensors will be used
  
  For this Project, an ESP 32 (NodeMCU) is used with 12 Bits ADCs
  The ESP 32 device works at 3.3 Volt levels, while the Moist Level Indicator runs on 5 V
  The 5 Level LEDs have been built in a separate indicator Display (indicating 1%, 25%, 50%, 75% and 100% levels)
  The ESP 32 is fed with 5 V power (from a 5V adaptor or 5v powerbank), it has an on-board 3.3V voltage regulator
  The 5 indicator LEDs get the 5V supply directly from the 5 volt pin of the ESP 32 
    
  This code is licensed under GPL3+ license.
*/

#define NUM_LEDS  5


///////////////////////////////////////////////// initialise the GPIO pins
const int IndicatorPin = 16;                    // pin 16 sends the control data to the LED Moist Level Indicator
const int ToneOutput1 = 25;                     // pin 25 is used for sending a 600 kHz PWM tone to the MoistSensor1 measurement circuit
const int freq = 600000;
const int Channel1 = 1;
const int resolution = 8;
const int MoistSensor1Pin = 4;                  // 12 bits ADC pin 4 senses the voltage level of the MoistSensor1 (values 0 - 4095)

int level = 0;                                  // Variable of the Moist Level: 1%, 25%, 50%, 75%, 100%
int Moistlevel1 = 0;                            // Variable to store the Moist level for sensor 1 




/////////////////////////////////////////////////// the setup code that follows, will run once after "Power On" or after a RESET
void setup() {
  Serial.begin(115200);
  ledcSetup(Channel1, freq, resolution);         // configure the PWM functionalitites
  ledcAttachPin(ToneOutput1, Channel1);           // attach the channel 1 to the GPIO pin 25 for generating a PWM signal of 600kHz
  
  pinMode(IndicatorPin, OUTPUT);                // Initializes the output pin for the Moist Level Indicator
  pinMode(MoistSensor1Pin, INPUT);               // Initializes the sensor pin (4) for measuring the MoistureLevelValue1
  
  strip.begin();                                 // Initialize all LEDs to "off"
  
  for (int t = 0; t < 5 ; t++)
    {
    strip.setPixelColor(t, 100, 100, 100);       // After Power On the Moist Level Indicator LEDs are tested once
    strip.show();                                // note that the order of colors of the WS2812 LED strip is R,G,B 
    delay (200);
    strip.setPixelColor(t, 0, 0, 0);             // and back to off
    }
  for (int k = 4; k > -1 ; k--)
    {
    strip.setPixelColor(k, 100, 100, 100);       // blink for 0,2 seconds going top down and then off
    strip.show();
    delay (200);
    strip.setPixelColor(k, 0, 0, 0);             // and back to off
    }
}


/////////////////////////////////////////////////// the loop code that follows, will run repeatedly until "Power Off" or a RESET
void loop(){

   MEASUREMOISTURE1 ();                     // measure moisture level1
  
   delay(1000);                            // wait 1 sec to Check for new values;
                                            //this value is just for demonstration purposes and will in a practical application be far less frequent
}
//////////////////END of LOOP////////////////////////////////////////////////////////////  

/////////////////////////////////////////////////// Hereafter follows the Function for measuring the Moisture level with MoistSensor1 (capacitive measurement)

void MEASUREMOISTURE1 ()  {
  Moistlevel1 = 0;
  ledcWrite(Channel1, 128);                             // send a PWM signal of 600 kHz to pin 25 with a dutycycle of 50%
  delay(200);                                           // allow the circuit to stabilize  
  for (int m = 1; m < 6 ; m++)                          // take 5 consecutive measurements in 5 seconds
    {
      Moistlevel1 = Moistlevel1 + analogRead(MoistSensor1Pin) ;    // Read data from analog pin 4 and add it to MoistLevel1 variable
      delay (1000);
    }
    Moistlevel1 = Moistlevel1 / 5;                       // Determine the average of 5 measurements 
    
    if (Moistlevel1 > 150 && Moistlevel1 < 375){
      level = 5;                                         // means moistlevel is 100%
      Serial.print(" Moistlevel is 100% "); Serial.print(" Moistlevel1 = "); Serial.println (Moistlevel1);
    }
    if (Moistlevel1 > 375 && Moistlevel1 < 725){
      level = 4;                                         // means moistlevel is 75%
      Serial.print(" Moistlevel is 75% "); Serial.print(" Moistlevel1 = "); Serial.println (Moistlevel1);
    }
    if (Moistlevel1 > 725 && Moistlevel1 < 1075){
      level = 3;                                         // means moistlevel is 50%
      Serial.print(" Moistlevel is 50% "); Serial.print(" Moistlevel1 = "); Serial.println (Moistlevel1);
    }
    if (Moistlevel1 > 1075 && Moistlevel1 < 1425){
      level = 2;                                         // means moistlevel is 25%
      Serial.print(" Moistlevel is 25% "); Serial.print(" Moistlevel1 = "); Serial.println (Moistlevel1);
    }
    if (Moistlevel1 > 1425 && Moistlevel1 < 1775){
      level = 1;                                         // means moistlevel is 1%
      Serial.print(" Moistlevel is 1% "); Serial.print(" Moistlevel1 = "); Serial.println (Moistlevel1);
    }
  ledcWrite(Channel1, 0);                                // stop generating PWM tones at pin 25
  }


/////////////////////////////////////////////////// Hereafter follows the Function for Indicating the MoistureLevel (called from within the loop)

