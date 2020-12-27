////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///****************************************LINEAMETEO STAZIONE SENDER********************************************///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////LIBRARY DECLARATION////////////////////////////////////////
#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>
#include "ClosedCube_SHT31D.h"
#include <Adafruit_SI1145.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <math.h>
#include "WiFiClient.h"
#include <String.h>
#include <Wire.h>
#include <SHT1x.h>
WiFiManager wifiManager;
WiFiClient  client;
//--------------------------------------------------------------------------------------//

//////////DEFINE SENSORS////////////////


//SHT1X PINS//
#define dataPin  12
#define clockPin 14
SHT1x sht1x(dataPin, clockPin);


//ANEMOMETER, RAIN GAUGE//
#define pinInterrupt 2 // rain gauge
#define WindSensorPin 0 // anemometer


//SHT3X DEFINE//
ClosedCube_SHT31D sht3xd;


///UV SENSOR DEFINE//
Adafruit_SI1145 uv = Adafruit_SI1145();


////////////////////*********FIREBASE DETAILS************///////////////////////////////////
#define FIREBASE_HOST ""                 // the project name address from firebase id
#define FIREBASE_AUTH ""            // the secret key generated from firebase
FirebaseData Weather;

//--------------------------------------------------------------------------------------//

/////TIMING VARIALABLES//////
unsigned long timeout; // waiting for WiFi connection
byte Sleep; // Sleep or not sleep variable
unsigned long previoustemp = 0;  // for millis() use
unsigned long previousMillis = 0; // for millis() use
const int sleepTimeS = 300; // sleeping time in seconds
const int halfawakeTimeS = 180; // sleeping time routine in awake mode in seconds
unsigned long timeawake = 7200000; // time awake in ms
unsigned long timecountSleep = 0; // delay sleep if rain or gust detected
unsigned long uploadtime; // pool data to server interval
boolean goingtosleep; //   true = sleeping      false = not sleeping
byte CurrentDay; // current day to reset max and min values
byte PreviousDay; // previous day to reset max and min values
byte alternate = 4; // minimize work when active WiFi
bool justrestart = false; // if just restart don't wait and upload data to database
bool res; // WiFi Status

//--------------------------------------------------------------------------------------//

////////////RAIN////////////////
float mmGoccia; // = 0.2; // tipping bucket count in mm
unsigned int gocce = 0; // tipping bucket movements
float mmPioggia = 0.0; // daily rain
volatile unsigned long time1; // rain rate timing calculation
unsigned long PluvioStep = 0; // rain rate timing calculation
unsigned long PluvioOldStep = 0; // rain rate timing calculation
volatile byte PluvioFlag = 0; // detect interrupt of rain
volatile unsigned long ContactBounceTimeRain = 0; // Timer to avoid double counting in interrupt
float rainrate = 0; // real-time rainrate
float rainrateMax = 0; // daily rainrateMax


////////////WIND/////////////////
volatile unsigned int Rotations; // cup rotation counter used in interrupt routine
volatile unsigned long ContactBounceTime = 0; // Timer to avoid contact bounce in interrupt routine
unsigned long calculation = 0; //millis() for sample time
const int average = 3000; // sample time for wind speed
float constant; // formula calculation
float WindSpeed; // speed km/h
float Gust = 0; // gust variable
int VaneValue; // raw analog value from wind vane
int Direction; // translated 0 - 360 direction
int CalDirection; // converted value with offset applied
int Offset; // adjust wind direction value
byte gustDetected = 0; // write on database the gust if detected


//////////////UV/////////////////
float UVindex; // variable UVindex


//////////TEMPERATURE///////////
//SHT1X//
float temp_c; // temperature in centigrade
float temp_f; // temperature in fahrenheit
int humiditySHT1x; // humidity variable

//SHT3X//
float temperatureSHT3x; // temperature in centigrade
int humiditySHT3x; // humidity variable

//DAILY MAX AND MIN TEMPERATURE//
float MaxTemp;
float MinTemp;

//--------------------------------------------------------------------------------------//

///////////////////////////////INTERRUPT ISR//////////////////////////////////////////////
void ICACHE_RAM_ATTR ContaGocce()
{
  if ((millis() - ContactBounceTimeRain) > 250 ) { // debounce the switch contact.
    PluvioFlag = 1;
    time1 = millis();
    ContactBounceTimeRain = millis();
  }
}

void ICACHE_RAM_ATTR isr_rotation () {
  if ((millis() - ContactBounceTime) > 15 ) { // debounce the switch contact.
    Rotations++;
    ContactBounceTime = millis();
  }
}

//--------------------------------------------------------------------------------------//


//////////////////////////////////SETUP FUNCTIONS////////////////////////////////////////
void initPioggia () {
  attachInterrupt(digitalPinToInterrupt(pinInterrupt), ContaGocce, FALLING);// pin interrupt per il pluviometro WS2300-16
}

void initVento () {
  pinMode(WindSensorPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(WindSensorPin), isr_rotation, FALLING);
  constant = 2.25 / (average / 1000);
}

void initData()
{
  //Serial.println("Getting Data...");
  if (Firebase.getFloat(Weather, "/Rain/mmGoccia"))
  {
    mmGoccia = Weather.floatData();
  }
  if (Firebase.getFloat(Weather, "/Rain/Rain24H"))
  {
    mmPioggia = Weather.floatData();
  }
  if (Firebase.getFloat(Weather, "/Wind/Gust24H"))
  {
    Gust = Weather.floatData();
  }
  if (Firebase.getFloat(Weather, "SHT3x/Temperature/TemperatureMax"))
  {
    MaxTemp = Weather.floatData();
  }
  if (Firebase.getFloat(Weather, "SHT3x/Temperature/TemperatureMin"))
  {
    MinTemp = Weather.floatData();
  }
  if (Firebase.getFloat(Weather, "/Rain/RainRateMax"))
  {
    rainrateMax = Weather.floatData();
  }
  if (Firebase.getInt(Weather, "/Wind/Offset"))
  {
    Offset = Weather.intData();
  }
  if (Firebase.getInt(Weather, "ChangeTime/PreviousDay"))
  {
    PreviousDay = Weather.intData();
  }
  if (Firebase.getInt(Weather, "/ChangeTime/SendDataTime"));
  {
    uploadtime = Weather.intData() * 1000;
  }
}

void nosleep()
{
  if (Firebase.getInt(Weather, "/Sleep"))
  {
    Sleep = Weather.intData();
  }
  if (Sleep == 1)
  {
    goingtosleep = false;
    //Serial.println("I'm halfawake now");
    initPioggia();
    initVento();
    initData();
  }
  else if (Sleep == 0)
  {
    goingtosleep = true;
  }
}

//--------------------------------------------------------------------------------------//

//////////////////////////////////LOOP FUNCTIONS//////////////////////////////////////////

//RAIN RATE//
void PluvioDataEngine() {
  if (((PluvioStep - PluvioOldStep) != 0) && (gocce >= 2)) {
    if ((millis() - PluvioStep) > (PluvioStep - PluvioOldStep)) {
      rainrate = 3600 / (((millis() - PluvioStep) / 1000)) * mmGoccia;
      if (rainrate < 1) {
        timecountSleep = millis();
        gocce = 0;
        rainrate = 0;
      }
    } else {
      rainrate = 3600 / (((PluvioStep - PluvioOldStep) / 1000)) * mmGoccia;
    }
  } else {
    rainrate = 0.0;
  }
}

//RAIN//
void readPioggia () {
  if (rainrate >= 0.2 && rainrate <= 0.6)
  {
    timecountSleep = millis();
    rainrate = 0;
    gocce = 0;
  }
  if (PluvioFlag == 1) {
    PluvioFlag = 0;
    PluvioOldStep = PluvioStep;
    PluvioStep = time1;
    timecountSleep = millis();
    gocce++; // incrementa numero basculate
    mmPioggia = mmPioggia + mmGoccia;
  }
  PluvioDataEngine();
}

//WIND//
void readVento () {
  //FORMULA V=P(Rotations)(2.25/T)--->constant
  if (millis() - calculation >= average)
  {
    calculation = millis();
    WindSpeed = (Rotations * constant) * 1.60934;
    Rotations = 0;
  }

  if (WindSpeed > Gust)
  {
    Gust = WindSpeed;
    gustDetected = 1;
    timecountSleep = millis();
  }

  VaneValue = analogRead(A0);
  Direction = map(VaneValue, 0, 1023, 0, 360);
  CalDirection = Direction + Offset;

  if (CalDirection > 360) {
    CalDirection = CalDirection - 360;
  }
  if (CalDirection < 0) {
    CalDirection = CalDirection + 360;
  }
}

//MAX AND MIN DAILY//
void maxminvalues()
{
  if ((temperatureSHT3x > MaxTemp) && (temperatureSHT3x < 100))
  {
    MaxTemp = temperatureSHT3x;
  }

  if (temperatureSHT3x < MinTemp)
  {
    MinTemp = temperatureSHT3x;
  }
  if (rainrate > rainrateMax)
  {
    rainrateMax = rainrate;
  }
}

//READ AND UPLOAD DATA//
void readData()
{
  temp_c = sht1x.readTemperatureC();
  //temp_f = sht1x.readTemperatureF();
  humiditySHT1x = sht1x.readHumidity();
  printResult("Pooling Mode", sht3xd.readTempAndHumidity(SHT3XD_REPEATABILITY_HIGH, SHT3XD_MODE_POLLING, 50));
  UVindex = uv.readUV();
  UVindex /= 100.0;  // to convert to integer index, divide by 100
  readPioggia();
  readVento();

  if ((millis() - previoustemp >= uploadtime) || (justrestart == true))
  {
    justrestart = false;
    WiFi.forceSleepWake();
    delay(100);
    // Bring up the WiFi connection
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    timeout = millis();
    while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      Serial.print(".");
      if (millis() - timeout > 35000)
      {
        //Serial.println("Going to sleep for 1 minute and try again from setup..");
        ESP.deepSleep(60 * 1000000, WAKE_RF_DEFAULT);
      }
    }
    long rssi = WiFi.RSSI();
    previoustemp = millis();


    if (Firebase.getInt(Weather, "ChangeTime/CurrentDay"))
    {
      CurrentDay = Weather.intData();
    }

    alternate++;
    Firebase.setFloat(Weather, "SHT3x/Temperature/Temperature", temperatureSHT3x);
    Firebase.setInt(Weather, "SHT3x/Humidity", humiditySHT3x);
    Firebase.setFloat(Weather, "/SHT1x/Temperature", temp_c);
    Firebase.setInt(Weather, "/SHT1x/Humidity", humiditySHT1x);
    Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMax", MaxTemp);
    Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMin", MinTemp);
    Firebase.setFloat(Weather, "/Wind/WindSpeed", WindSpeed);
    Firebase.setInt(Weather, "/Wind/WindDirection", CalDirection);

    if (alternate > 4)
    {
      alternate = 0;
      Firebase.setInt(Weather, "/UVindex", UVindex);
      Firebase.setInt(Weather, "/Connection/WiFiSignaldb", rssi);
    }

    if (gustDetected == 1)
    {
      gustDetected = 0;
      Firebase.setFloat(Weather, "/Wind/Gust24H", Gust);
    }

    if (mmPioggia > 0)
    {
      Firebase.setFloat(Weather, "/Rain/Rain24H", mmPioggia);
      Firebase.setFloat(Weather, "/Rain/RainRate", rainrate);
    }
    if (rainrate > 0)
    {
      Firebase.setFloat(Weather, "/Rain/RainRateMax", rainrateMax);
    }
    if (CurrentDay != PreviousDay && CurrentDay != 0)
    {
      PreviousDay = CurrentDay;
      MaxTemp = temperatureSHT3x;
      MinTemp = temperatureSHT3x;
      mmPioggia = 0;
      Gust = 0;
      rainrateMax = 0;
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMax", temperatureSHT3x);
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMin", temperatureSHT3x);
      Firebase.setFloat(Weather, "/Wind/Gust24H", 0);
      Firebase.setFloat(Weather, "/Rain/Rain24H", 0);
      Firebase.setFloat(Weather, "/Rain/RainRateMax", 0);
      Firebase.setInt(Weather, "ChangeTime/PreviousDay", PreviousDay);
    }
    sleepAwakeFunction();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(100);
  }
}

//SLEEPING AFTER AWAKE PERIOD//
void sleepAwakeFunction()
{
  if ((millis() - timecountSleep >= timeawake) && (rainrate == 0))
  {
    //Serial.println("Going to Sleep..");
    timecountSleep = millis();
    ESP.deepSleep(halfawakeTimeS * 1000000, WAKE_RF_DEFAULT);
  }
}

//READ SHT3X//
void printResult(String text, SHT31D result) {
  if (result.error == SHT3XD_NO_ERROR) {
    temperatureSHT3x = result.t;
    humiditySHT3x = result.rh;
  } else {
    //Serial.print(text);
    //Serial.print(": [ERROR] Code #");
    //Serial.println(result.error);
  }
}

//--------------------------------------------------------------------------------------//

////////////////////////////////SLEEPING FUNCTIONS///////////////////////////////////////

void readDataSleeping ()
{
  temp_c = sht1x.readTemperatureC();
  //temp_f = sht1x.readTemperatureF();
  humiditySHT1x = sht1x.readHumidity();
  printResult("Pooling Mode", sht3xd.readTempAndHumidity(SHT3XD_REPEATABILITY_HIGH, SHT3XD_MODE_POLLING, 50));
  UVindex = uv.readUV();
  UVindex /= 100.0;  // to convert to integer index, divide by 100
}

void writeDataSleeping()
{
  Firebase.setFloat(Weather, "SHT3x/Temperature/Temperature", temperatureSHT3x);
  Firebase.setInt(Weather, "SHT3x/Humidity", humiditySHT3x);
  Firebase.setFloat(Weather, "/SHT1x/Temperature", temp_c);
  Firebase.setInt(Weather, "/SHT1x/Humidity", humiditySHT1x);
  Firebase.setInt(Weather, "/UVindex", UVindex);
  Firebase.setInt(Weather, "Connection/WiFiSignaldb", WiFi.RSSI());
  ESP.deepSleep(sleepTimeS * 1000000, WAKE_RF_DEFAULT);
}


void setup() {
  //Serial.begin(115200);  // Initialize serial
  //wifiManager.resetSettings();
  WiFi.mode(WIFI_STA);
  wifiManager.setConfigPortalTimeout(100);

  WiFi.begin();
  timeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - timeout > 35000)
    {
      res = wifiManager.autoConnect("LineaMeteoStazioneS", "LaMeteo2005");

      if (!res) {
        Serial.println("Failed to connect");
        ESP.deepSleep(60 * 1000000, WAKE_RF_DEFAULT);
      }
    }
  }

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);   // connect to firebase
  Firebase.reconnectWiFi(true);
  Firebase.setMaxRetry(Weather, 2);
  nosleep();
  Wire.pins(4, 5); // SDA=>D2  SCL=>D1
  Wire.begin();
  sht3xd.begin(0x44);
  sht3xd.readSerialNumber();
  uv.begin();                           // initializes the sensor
  justrestart = true;
  /*if (sht3xd.periodicStart(SHT3XD_REPEATABILITY_HIGH, SHT3XD_FREQUENCY_10HZ) != SHT3XD_NO_ERROR)
    Serial.println("[ERROR] Cannot start periodic mode");*/
}

void loop() {
  if (goingtosleep == false)
  {
    readData();
    maxminvalues();
  }
  if (goingtosleep == true)
  {
    readDataSleeping();
    writeDataSleeping();
  }
}
