////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///****************************************LINEAMETEO STAZIONE VISUAL********************************************///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////LIBRARY DECLARATION////////////////////////////////////////
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in
#include <JSON_Decoder.h>
#include <OpenWeather.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
WiFiManager wifiManager;
#include "NTPClient.h"
#include <WiFiUDP.h>
#include <FirebaseESP32.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "epaper_fonts.h"
#include "lang.h"
#include "common_functions.h"
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"


////////////////////*********FIREBASE DETAILS************///////////////////////////////////
#define FIREBASE_HOST ""                 // the project name address from firebase id
#define FIREBASE_AUTH ""            // the secret key generated from firebase
FirebaseData Weather;


//////////////////////*********NTP SERVER************//////////////////////////////////////
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS "pool.ntp.org" // "ca.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)
// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS , NTP_OFFSET, NTP_INTERVAL);


////////////////////*********OPENWEATHER DETAILS************////////////////////////////////
String api_key;
String latitude; // 90.0000 to -90.0000 negative for Southern hemisphere
String longitude; // 180.000 to -180.000 negative for West
String units = "metric";
String language;   // See notes tab
String Hemisphere;

OW_Weather ow; // Weather forecast library instance


////////////////////*********DISPLAY DEFINITION************///////////////////////////////////
#define SCREEN_WIDTH  400.0    // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 300.0
#define  ENABLE_GxEPD2_display 0

enum alignment {LEFT, RIGHT, CENTER};

// Connections for e.g. LOLIN D32
static const uint8_t EPD_BUSY = 4;  // to EPD BUSY
static const uint8_t EPD_CS   = 5;  // to EPD CS
static const uint8_t EPD_RST  = 16; // to EPD RST
static const uint8_t EPD_DC   = 17; // to EPD DC
static const uint8_t EPD_SCK  = 18; // to EPD CLK
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23; // to EPD DIN

// Connections for e.g. Waveshare ESP32 e-Paper Driver Board
//static const uint8_t EPD_BUSY = 25;
//static const uint8_t EPD_CS   = 15;
//static const uint8_t EPD_RST  = 26;
//static const uint8_t EPD_DC   = 27;
//static const uint8_t EPD_SCK  = 13;
//static const uint8_t EPD_MISO = 12; // Master-In Slave-Out not used, as no data from display
//static const uint8_t EPD_MOSI = 14;

GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

// Using fonts:
// u8g2_font_helvB08_tf
// u8g2_font_helvB10_tf
// u8g2_font_helvB12_tf
// u8g2_font_helvB14_tf
// u8g2_font_helvB18_tf
// u8g2_font_helvB24_tf

boolean LargeIcon = true, SmallIcon = false;
#define Large  9           // For icon drawing, needs to be odd number for best effect
#define Small  9            // For icon drawing, needs to be odd number for best effect
#define moon 13

//LANGUAGE//
String WIFI;
String DEWPOINT = "DewPoint   ";
String UVINDEX = "UVIndex  ";
String HEATINDEX = "HeatIndex   ";
String UPDATE = "Last Updated On";
String INSIDE = "INSIDE";
String RAIN = "     Rain24H";
String RAINRATE = "  RainRate";
String RAINRATEMAX = "  RRMax";
String GUST = "   Gust";


//////////DEFINE PRESSURE SENSOR AND DATA////////////////
Adafruit_BME680 bme; // I2C
float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score
float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;
int AQIndex;
String AQI;
//int CALIBRATION;
//float pressure;
float pressurehpa;
//PRESSURE EXTREMES//
//float pressureMax;
//float pressureMin;


//######################## PROGRAM VARIABLES AND OBJECTS ##################################//


//DEWPOINT CALCULATION CONSTANTS//
#define c1 -8.78469475556
#define c2 1.61139411
#define c3 2.33854883889
#define c4 -0.14611605
#define c5 -0.012308094
#define c6 -0.0164248277778
#define c7 0.002211732
#define c8 0.00072546
#define c9 -0.000003582


//////////TEMPERATURE///////////
float temp;
float temperatureIN;
float maxTemp;
float minTemp;
float maxTempSleep;
float minTempSleep;


////////////HUMIDITY////////////
int humidity;
int humidityIN;
//int maxHumidity;
//int minHumidity;
float dewPoint; // variabile del punto di rugiada
float heatIndex; // variabile dell'indice di calore


////////////////UV//////////////
int UVindex;


//////////////WIND//////////////
float WindSpeed;
float windchill; // variabile del raffreddamento causato dal vento
float CalDirection;
float Gust;


//////////////RAIN//////////////
float rainrate;
float mmPioggia;
float rainrateMax;


/////TIMING VARIALABLES//////
unsigned long timeout;
bool res;
int TIMEZONE;
long TIMEZONEINSECONDS;


//////SLEEP DEFINITIONS///////
int SleepDuration;


////--------------------------------------------------------------------------------------////
////--------------------------------------------------------------------------------------////


//////////////////////////////////SETUP FUNCTIONS////////////////////////////////////////

void getDataTime()
{
  if (Firebase.getInt(Weather, "ChangeTime/TIMEZONE"))
  {
    TIMEZONE = Weather.intData();
  }
  /*if (Firebase.getInt(Weather, "Pressure/Calibration"))
  {
    CALIBRATION = Weather.intData();
  }*/
  if (Firebase.getString(Weather, "Services/OpenWeather/API"))
  {
    api_key = Weather.stringData();
  }
  if (Firebase.getString(Weather, "Services/OpenWeather/Language"))
  {
    language = Weather.stringData();
  }
  if (Firebase.getString(Weather, "Services/OpenWeather/Latitude"))
  {
    latitude = Weather.stringData();
  }
  if (Firebase.getString(Weather, "Services/OpenWeather/Longitude"))
  {
    longitude = Weather.stringData();
  }
  if (Firebase.getString(Weather, "Services/OpenWeather/Hemisphere"))
  {
    Hemisphere = Weather.stringData();
  }
  /*Serial.println(language);
    Serial.println(latitude);
    Serial.println(longitude);
    Serial.println(api_key);
    Serial.println(Hemisphere);*/
}

////--------------------------------------------------------------------------------------////

void InitialiseDisplay() {
  display.init(0);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  u8g2Fonts.begin(display); // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}

////--------------------------------------------------------------------------------------////


//////////////////////////////////TIME FUNCTIONS////////////////////////////////////////

void gettime()
{
  TIMEZONEINSECONDS = TIMEZONE * 3600;
  timeClient.update();
  timeClient.setTimeOffset(TIMEZONEINSECONDS);
  timeClient.getFullFormattedTime();
}

//SLEEP//
void BeginSleep() {
  display.powerOff();
  if (timeClient.getHours() >= 1 && timeClient.getHours() < 7)
  {
    SleepDuration = 7200; //2 hours at night
  }
  else
  {
    SleepDuration = 1200; //20 minutes
  }
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1); //1 = High, 0 = Low
  esp_sleep_enable_timer_wakeup(SleepDuration * 1000000LL);
  esp_deep_sleep_start();
}

////--------------------------------------------------------------------------------------////


//////////////////////////////////GETTING DATA FUNCTIONS////////////////////////////////////////


void airQuality() {
  //Calculate humidity contribution to IAQ index
  //int AQIndex;
  float current_humidity = bme.readHumidity();
  if (current_humidity >= 38 && current_humidity <= 42)
    hum_score = 0.25 * 100; // Humidity +/-5% around optimum
  else
  { //sub-optimal
    if (current_humidity < 38)
      hum_score = 0.25 / hum_reference * current_humidity * 100;
    else
    {
      hum_score = ((-0.25 / (100 - hum_reference) * current_humidity) + 0.416666) * 100;
    }
  }

  //Calculate gas contribution to IAQ index
  float gas_lower_limit = 5000;   // Bad air quality limit
  float gas_upper_limit = 50000;  // Good air quality limit
  if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit;
  if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
  gas_score = (0.75 / (gas_upper_limit - gas_lower_limit) * gas_reference - (gas_lower_limit * (0.75 / (gas_upper_limit - gas_lower_limit)))) * 100;

  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = hum_score + gas_score;

  //Serial.println("Air Quality = " + String(air_quality_score, 1) + "% derived from 25% of Humidity reading and 75% of Gas reading - 100% is good quality air");
  //Serial.println("Humidity element was : " + String(hum_score / 100) + " of 0.25");
  //Serial.println("     Gas element was : " + String(gas_score / 100) + " of 0.75");
  if (bme.readGas() < 120000) //Serial.println("***** Poor air quality *****");
    //Serial.println();
    if ((getgasreference_count++) % 10 == 0) GetGasReference();
  //Serial.println(CalculateIAQ(air_quality_score));
  //Serial.println("------------------------------------------------");
  //delay(2000);

  AQIndex = (100 - air_quality_score) * 5;
  AQI = CalculateIAQ(air_quality_score);
  //Firebase.setString(Weather, "AirQuality/Condition", CalculateIAQ(air_quality_score));
  //Firebase.setInt(Weather, "AirQuality/Score", AQIndex);
}

void GetGasReference() {
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
  //Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++) { // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

String CalculateIAQ(float score) {
  if (language == "it")
  {
    String IAQ_testo = "Aria ";
    score = (100 - score) * 5;
    if      (score >= 300)                 IAQ_testo += "Pericolosa";
    else if (score >= 200 && score < 300 ) IAQ_testo += "Pessima";
    else if (score >= 150 && score < 200 ) IAQ_testo += "Scadente";
    else if (score >= 100 && score < 150 ) IAQ_testo += "Mediocre";
    else if (score >=  50 && score < 100 ) IAQ_testo += "Accettabile";
    else if (score >=  00 && score <  50 ) IAQ_testo += "Ottima";
    return IAQ_testo;
  }
  else
  {
    String IAQ_text = "Air ";
    score = (100 - score) * 5;
    if      (score >= 300)                 IAQ_text += "Hazardous";
    else if (score >= 200 && score < 300 ) IAQ_text += "Very Unhealthy";
    else if (score >= 150 && score < 200 ) IAQ_text += "Unhealthy";
    else if (score >= 100 && score < 150 ) IAQ_text += "Unhealthy for Sensitive Groups";
    else if (score >=  50 && score < 100 ) IAQ_text += "quality is Moderate";
    else if (score >=  00 && score <  50 ) IAQ_text += "is Good";
    return IAQ_text;
  }
}


void getValues()
{
  gettime();
  if (Firebase.getFloat(Weather, "SHT3x/Temperature/Temperature"))
  {
    temp = Weather.floatData();
  }

  if (Firebase.getInt(Weather, "SHT3x/Humidity"))
  {
    humidity = Weather.intData();
  }

  if (Firebase.getFloat(Weather, "Rain/RainRate"))
  {
    rainrate = Weather.floatData();
  }

  if (Firebase.getFloat(Weather, "Wind/WindSpeed"))
  {
    WindSpeed = Weather.floatData();
  }

  if (Firebase.getFloat(Weather, "Wind/WindDirection"))
  {
    CalDirection = Weather.intData();
  }

  if (Firebase.getFloat(Weather, "Pressure/Pressure"))
  {
    pressurehpa = Weather.floatData();
  }
  if (Firebase.getInt(Weather, "/UVindex"))
  {
    UVindex = Weather.intData();
  }
  bme.performReading();
  //pressure = bme.pressure + CALIBRATION;
  //pressurehpa = pressure / 100;
  humidityIN = bme.humidity;
  temperatureIN = bme.temperature;
  //Firebase.setFloat(Weather, "Pressure/Pressure", pressurehpa);
  Firebase.setFloat(Weather, "Inside/Temperature", temperatureIN);
  Firebase.setInt(Weather, "Inside/Humidity", humidityIN);
  airQuality();

  dewPoint = (temp - (14.55 + 0.114 * temp) * (1 - (0.01 * humidity)) - pow(((2.5 + 0.007 * temp) * (1 - (0.01 * humidity))), 3) - (15.9 + 0.117 * temp) * pow((1 - (0.01 * humidity)), 14));  //equazione per il calcolo del dewpoint
  if (temp > 21)
  {
    heatIndex = c1 + (c2 * temp) + (c3 * humidity) + (c4 * temp * humidity) + (c5 * sq(temp)) + (c6 * sq(humidity)) + (c7 * sq(temp) * humidity) + (c8 * temp * sq(humidity)) + (c9 * sq(temp) * sq(humidity));
  }
  else if (temp <= 21)
  {
    heatIndex = temp;
  }
  if (temp < 11)
  {
    windchill = (13.12 + 0.6215 * temp) - (11.37 * pow(WindSpeed, 0.16)) + (0.3965 * temp * pow(WindSpeed, 0.16));//equazione per il calcolo del windchill
  }
  else
  {
    windchill = temp;
  }

  if (language == "it")
  {
    DEWPOINT = "Punto di Rugiada  ";
    UVINDEX = "Indice UV  ";
    HEATINDEX = "Indice di Calore  ";
    UPDATE = "Ultimo Aggiornamento";
    INSIDE = "INTERNA";
    RAIN = "    Pioggia24H";
    RAINRATE = " Intensità";
    RAINRATEMAX = " IntensitàMax";
    GUST = "  RafficaMax";
  }
  maxminvalues();
}


void maxminvalues()
{
  if (Firebase.getFloat(Weather, "SHT3x/Temperature/TemperatureMax"))
  {
    maxTemp = Weather.floatData();
    maxTempSleep = maxTemp;
  }

  if (Firebase.getFloat(Weather, "SHT3x/Temperature/TemperatureMin"))
  {
    minTemp = Weather.floatData();
    minTempSleep = minTemp;
  }

  if (Firebase.getFloat(Weather, "Rain/Rain24H"))
  {
    mmPioggia = Weather.floatData();
  }

  if (Firebase.getFloat(Weather, "Wind/Gust24H"))
  {
    Gust = Weather.floatData();
  }

  if (Firebase.getFloat(Weather, "Rain/RainRateMax"))
  {
    rainrateMax = Weather.floatData();
  }
}

////--------------------------------------------------------------------------------------////


//////////////////////////////////DISPLAY FUNCTIONS//////////////////////////////////////////


void DisplayWeather() {                 // 4.2" e-paper display is 400x300 resolution
  DrawHeadingSection();                 // Top line of the display
  DrawMainWeatherSection(172, 70);      // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  DisplayPrecipitationSection(233, 82); // Precipitation section
}

//HEADING//
void DrawHeadingSection() {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  if (language == "it")
  {
    if (WiFi.RSSI() > -55)
    {
      WIFI = "Segnale Ottimo";
      drawString(SCREEN_WIDTH / 2, 0,  WIFI, CENTER);
    }
    if (WiFi.RSSI() <= -55 && WiFi.RSSI() > -70)
    {
      WIFI = "Segnale Buono";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
    if (WiFi.RSSI() <= -70 && WiFi.RSSI() > -85)
    {
      WIFI = "Segnale Accettabile";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
    if (WiFi.RSSI() <= -85)
    {
      WIFI = "Segnale Scarso";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
  }
  else
  {
    if (WiFi.RSSI() > -55)
    {
      WIFI = "WiFi Signal Excellent";
      drawString(SCREEN_WIDTH / 2, 0,  WIFI, CENTER);
    }
    if (WiFi.RSSI() <= -55 && WiFi.RSSI() > -70)
    {
      WIFI = "WiFi Signal Good";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
    if (WiFi.RSSI() <= -70 && WiFi.RSSI() > -85)
    {
      WIFI = "WiFi Signal Fair";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
    if (WiFi.RSSI() <= -85)
    {
      WIFI = "WiFi Signal Weak";
      drawString(SCREEN_WIDTH / 2, 0, WIFI , CENTER);
    }
  }


  drawString(SCREEN_WIDTH, 0, "LineaMeteoStazione", RIGHT);
  DrawBattery(25, 12);
  display.drawLine(0, 12, SCREEN_WIDTH, 12, GxEPD_BLACK);
}

//MAIN//
void DrawMainWeatherSection(int x, int y) {
  OW_current *current = new OW_current;
  OW_hourly *hourly = new OW_hourly;
  OW_daily  *daily = new OW_daily;

  ow.getForecast(current, hourly, daily, api_key, latitude, longitude, units, language);

  DisplayDisplayWindSection(x - 115, y - 3, 40);
  DisplayWXicon(x + 5, y - 5);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  DrawPressureAndTrend(x - 142, y + 58);

  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 167, y + 120,  UVINDEX + String(UVindex), LEFT);

  if (language == "it")
  {
    if (UVindex < 3)
    {
      drawString(x - 82, y + 120,  + " BASSO", LEFT);
    }
    if (UVindex >= 3 && UVindex < 6)
    {
      drawString(x - 77, y + 120,  + " MEDIO", LEFT);
    }

    if (UVindex >= 6 && UVindex < 9)
    {
      drawString(x - 82, y + 120,  + " ALTO", LEFT);
    }

    if (UVindex >= 9)
    {
      drawString(x - 77, y + 120,  + " ESTREMO", LEFT);
    }
  }
  else
  {
    if (UVindex < 3)
    {
      drawString(x - 82, y + 120,  + "LOW", LEFT);
    }
    if (UVindex >= 3 && UVindex < 6)
    {
      drawString(x - 77, y + 120,  + "MEDIUM", LEFT);
    }

    if (UVindex >= 6 && UVindex < 9)
    {
      drawString(x - 82, y + 120,  + "HIGH", LEFT);
    }

    if (UVindex >= 9)
    {
      drawString(x - 77, y + 120,  + "EXTREME", LEFT);
    }
  }
  if (language == "it")
  {
    drawString(x - 167, y + 140,  + "IQA  " + String(AQIndex), LEFT);
  }
  else
  {
    drawString(x - 167, y + 140,  + "AQI  " + String(AQIndex), LEFT);
  }
  drawString(x - 110, y + 140, AQI, LEFT);
  drawString(x - 167, y + 160, HEATINDEX + String(heatIndex, 1) + "°C" , LEFT);
  drawString(x - 167, y + 180,  + "WindChill   " + String(windchill, 1) + "°C", LEFT);
  drawString(x - 167, y + 200,  DEWPOINT + String(dewPoint, 1) + "°C", LEFT);

  if (language == "it")
  {
    if (heatIndex > 27 && heatIndex <= 32)
    {
      drawString(x - 10, y + 160,  + "   Cautela", LEFT);
    }
    if (heatIndex > 32 && heatIndex <= 39)
    {
      drawString(x - 10, y + 160,  + "   Estrema Cautela", LEFT);
    }

    if (heatIndex > 39 && heatIndex <= 51)
    {
      drawString(x - 10, y + 160,  + "   Pericolo", LEFT);
    }

    if (heatIndex > 51)
    {
      drawString(x - 10, y + 160,  + "   Estremo Pericolo", LEFT);
    }
  }
  else
  {
    if (heatIndex > 27 && heatIndex <= 32)
    {
      drawString(x - 40, y + 160,  + "  Caution", LEFT);
    }
    if (heatIndex > 32 && heatIndex <= 39)
    {
      drawString(x - 40, y + 160,  + "  Extreme Caution", LEFT);
    }

    if (heatIndex > 39 && heatIndex <= 51)
    {
      drawString(x - 40, y + 160,  + "  Danger", LEFT);
    }

    if (heatIndex > 51)
    {
      drawString(x - 40, y + 160,  + "  Extreme Danger", LEFT);
    }
  }


  u8g2Fonts.setFont(u8g2_font_helvB12_tf);

  for (int i = 0; i < MAX_DAYS; i++)
  {
    drawStringMaxWidth(x - 170, y + 90, 28, current->description , LEFT);
    //Serial.println(current->description);
  }

  DrawMainWx(x, y + 60);
  display.drawRect(0, y + 70, 232, 38, GxEPD_BLACK);
  DrawAstronomySection(233, 94);
  delete current;
  delete hourly;
  delete daily;
}

//MAIN//
void DrawMainWx(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 9, y - 22, String(temp, 1) + "°" + ("C"), CENTER); // Show current Temperature
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawString(x + 12, y - 3, String(maxTempSleep, 1) + "°| " + String(minTempSleep, 1) + "°", CENTER); // Show forecast high and Low
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 80, y - 3, String(humidity) + "%", CENTER);
  if (language == "it")
  {
    drawString(x - 55, y - 3, "UR", CENTER);
  }
  else
  {
    drawString(x - 55, y - 3, "RH", CENTER);
  }
  //u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  //drawString(x + 20, y - 4, String(maxHumidity) + "%|" + String(minHumidity) + "%", CENTER); // Show forecast high and Low


  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  if (language == "it")
  {
    drawString(x + 52, y - 105, UPDATE, LEFT);
  }
  else
  {
    drawString(x + 65, y - 105, UPDATE, LEFT);
  }
  drawString(x + 65, y - 85, timeClient.getFullFormattedTime(), LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(x + 100, y + 35, INSIDE, LEFT);
  drawString(x + 80, y + 58, String(temperatureIN, 1) + "°C | " + String(humidityIN) + "%", LEFT); // Show forecast high and Low

  if (language == "it")
  {
    drawString(x + 192, y + 58, "UR", LEFT);
  }
  else
  {
    drawString(x + 192, y + 58, "RH", LEFT);
  }
}

//WIND//
void DisplayDisplayWindSection(int x, int y, int Cradius) {
  arrow(x, y, Cradius - 7, CalDirection, 12, 18); // Show wind direction on outer circle of width and length
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  int dxo, dyo, dxi, dyi;
  display.drawLine(0, 15, 0, y + Cradius + 30, GxEPD_BLACK);
  display.drawCircle(x, y, Cradius, GxEPD_BLACK);     // Draw compass circle
  display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
  display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45)  drawString(dxo + x + 10, dyo + y - 10, TXT_NE, CENTER);
    if (a == 135) drawString(dxo + x + 7,  dyo + y + 5,  TXT_SE, CENTER);
    if (a == 225) drawString(dxo + x - 15, dyo + y,      TXT_SW, CENTER);
    if (a == 315) drawString(dxo + x - 15, dyo + y - 10, TXT_NW, CENTER);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
  }
  drawString(x, y - Cradius - 10,     TXT_N, CENTER);
  drawString(x, y + Cradius + 5,      TXT_S, CENTER);
  drawString(x - Cradius - 10, y - 3, TXT_W, CENTER);
  drawString(x + Cradius + 8,  y - 3, TXT_E, CENTER);
  drawString(x - 2, y - 20, WindDegToDirection(CalDirection), CENTER);
  drawString(x + 3, y + 12, String(CalDirection, 0) + "°", CENTER);
  drawString(x + 3, y - 3, String(WindSpeed, 1) + ("km/h"), CENTER);
}
//#########################################################################################//
String WindDegToDirection(float CalDirection) {
  if (CalDirection >= 348.75 || CalDirection < 11.25)  return TXT_N;
  if (CalDirection >=  11.25 && CalDirection < 33.75)  return TXT_NNE;
  if (CalDirection >=  33.75 && CalDirection < 56.25)  return TXT_NE;
  if (CalDirection >=  56.25 && CalDirection < 78.75)  return TXT_ENE;
  if (CalDirection >=  78.75 && CalDirection < 101.25) return TXT_E;
  if (CalDirection >= 101.25 && CalDirection < 123.75) return TXT_ESE;
  if (CalDirection >= 123.75 && CalDirection < 146.25) return TXT_SE;
  if (CalDirection >= 146.25 && CalDirection < 168.75) return TXT_SSE;
  if (CalDirection >= 168.75 && CalDirection < 191.25) return TXT_S;
  if (CalDirection >= 191.25 && CalDirection < 213.75) return TXT_SSW;
  if (CalDirection >= 213.75 && CalDirection < 236.25) return TXT_SW;
  if (CalDirection >= 236.25 && CalDirection < 258.75) return TXT_WSW;
  if (CalDirection >= 258.75 && CalDirection < 281.25) return TXT_W;
  if (CalDirection >= 281.25 && CalDirection < 303.75) return TXT_WNW;
  if (CalDirection >= 303.75 && CalDirection < 326.25) return TXT_NW;
  if (CalDirection >= 326.25 && CalDirection < 348.75) return TXT_NNW;
  return " ? ";
}
//#########################################################################################//
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize - 10) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize - 10) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;           float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}

//PRESSURE//
void DrawPressureAndTrend(int x, int y) {
  drawString(x, y, String(pressurehpa, 1) + ("hPa"), CENTER);
}


//RAIN//
void DisplayPrecipitationSection(int x, int y) {
  display.drawRect(x, y - 15, 167, 85, GxEPD_BLACK); // precipitation outline
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 5, y - 5, String(mmPioggia, 1) + ("mm") + String (RAIN), LEFT);
  drawString(x + 5, y + 15, String(rainrate, 1) + ("mm/h") +  String(RAINRATE), LEFT);
  drawString(x + 5, y + 35, String(rainrateMax, 1) + ("mm/h") + String(RAINRATEMAX), LEFT);
  drawString(x + 5, y + 55, String(Gust, 1) + ("km/h") + String(GUST), LEFT);
}


//MOON AND ASTRONOMY//
void DrawAstronomySection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  display.drawRect(x + 15, y + 150, 150, 53, GxEPD_BLACK);
  const int day_utc   = timeClient.getDate();
  const int month_utc = timeClient.getMonth() - 1;
  const int year_utc  = timeClient.getYear();
  //Serial.println(month_utc);
  drawString(x + 120, y + 175, MoonPhase(day_utc, month_utc, year_utc), RIGHT);
  DrawMoon(x + 105, y + 140, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################//
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 38;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= 45; Ypos++) {
    double Xpos = sqrt(45 * 45 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = - Xpos;
      Xpos2 = (Rpos - 2 * Phase * Rpos - Xpos);
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = (Xpos - 2 * Phase * Rpos + Rpos);
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos)  / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines)  / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
}
//#########################################################################################//
String MoonPhase(int d, int m, int y) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6  * m;

  jd  = c + e + d - 694039.09;     /* jd is total days elapsed */
  jd /= 29.53059;                  /* divide by the moon cycle (29.53 days) */
  b   = jd;                        /* int(jd) -> b, take integer part of jd */
  jd -= b;                         /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;              /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                     /* 0 and 8 are the same phase so modulo 8 for 0 */
  Hemisphere.toLowerCase();

  if (language == "it")
  {
    if (Hemisphere == "south" || Hemisphere == "north") b = 7 - b;
    if (b == 0) return "Luna Nuova";              // New;              0%  illuminated
    if (b == 1) return "Luna Calante"; // Waxing crescent; 25%  illuminated
    if (b == 2) return "Ultimo Quarto";   // First quarter;   50%  illuminated
    if (b == 3) return "Gibbosa Calante";    // Waxing gibbous;  75%  illuminated
    if (b == 4) return "Luna Piena";             // Full;            100% illuminated
    if (b == 5) return "Gibbosa Crescente"; // Waning gibbous;  75%  illuminated
    if (b == 6) return "Primo Quarto";    // Third quarter;   50%  illuminated
    if (b == 7) return "Luna Crescente";  // Waning crescent; 25%  illuminated
    return "";
  }
  else
  {
    if (Hemisphere == "south" || Hemisphere == "north") b = 7 - b;
    if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
    if (b == 1) return TXT_MOON_WANING_CRESCENT;  // Waxing crescent; 25%  illuminated
    if (b == 2) return TXT_MOON_THIRD_QUARTER;    // First quarter;   50%  illuminated
    if (b == 3) return TXT_MOON_WANING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
    if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
    if (b == 5) return TXT_MOON_WAXING_GIBBOUS;   // Waning gibbous;  75%  illuminated
    if (b == 6) return TXT_MOON_FIRST_QUARTER;    // Third quarter;   50%  illuminated
    if (b == 7) return TXT_MOON_WAXING_CRESCENT;  // Waning crescent; 25%  illuminated
    return "";
  }
}


//ICONS//
void DisplayWXicon(int x, int y) {
  String IconName;
  bool IconSize;
  if (pressurehpa > 1020 && dewPoint > -1 && heatIndex < 33 && rainrate == 0 && humidity <= 94)                                 Sunny(x, y, IconSize, IconName);
  else if (pressurehpa >= 1013 && pressurehpa <= 1020 && dewPoint > -1 && heatIndex < 33 && rainrate == 0 && humidity <= 94)    MostlySunny(x, y, IconSize, IconName);
  else if (pressurehpa >= 1009 && pressurehpa < 1013 && dewPoint > -1 && heatIndex < 33 && rainrate == 0 && humidity <= 94)     MostlyCloudy(x, y, IconSize, IconName);
  else if (pressurehpa >= 1005 && pressurehpa < 1009 && dewPoint > -1 && heatIndex < 33 && rainrate == 0 && humidity <= 94)     Cloudy(x, y, IconSize, IconName);
  else if (pressurehpa < 1005 && dewPoint > -1 && heatIndex < 33 && rainrate == 0 && humidity <= 94)                            ChanceRain(x, y, IconSize, IconName);
  else if (rainrate > 0 && rainrate < 15 && dewPoint > -1)                                                                      Rain(x, y, IconSize, IconName);
  else if (rainrate >= 15 && dewPoint > -1)                                                                                     Tstorms(x, y, IconSize, IconName);
  else if (dewPoint <= -1 && pressurehpa >= 1005)                                                                               ChanceSnow(x, y, IconSize, IconName);
  else if (dewPoint <= -1 && pressurehpa < 1005)                                                                                Snow(x, y, IconSize, IconName);
  else if (heatIndex >= 33 && humidity <= 94 || AQIndex >= 90  && humidity <= 94)                                               Haze(x, y, IconSize, IconName);
  else if (humidity > 94)                                                                                                       Fog(x, y, IconSize, IconName);
  else                                                                                                                          Nodata(x, y, IconSize, IconName);
}


//#########################################################################################
void drawString(int x, int y, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}


//#########################################################################################
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
  int16_t  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (align == RIGHT)  x = x - w;
  if (align == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2) {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim(); // Remove any leading spaces
    u8g2Fonts.println(secondLine);
  }
}


//#########################################################################################//
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                      // Left most circle
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                      // Right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);            // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK); // Right middle upper circle
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK); // Upper and lower lines
  //Clear cloud inner
  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear left most circle
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);           // Clear right most circle
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE); // left middle upper circle
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE); // Right middle upper circle
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE); // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
  }
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  x = x + 12; y = y + 12;
  display.fillCircle(x - 18, y - 20, scale, GxEPD_BLACK);
  display.fillCircle(x - 3, y - 20, scale * 1.6, GxEPD_WHITE);
}


//DISPLAY WEATHER STATUS//
void Sunny(int x, int y, bool IconSize, String IconName) {

  int scale = Large, offset = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y + offset, moon, IconSize);
  }
  else
  {
    scale = scale * 1.6;
    addsun(x, y, scale, IconSize);
  }
}


//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  //Serial.println(timeClient.getHours());
  int scale = Large, linesize = 1, offset = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x - moon * 1.8, y - moon * 1.8 + offset, moon, IconSize);
    addcloud(x, y + offset, scale, linesize);
  }
  else
  {
    addcloud(x, y + offset, scale, linesize);
    addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
  }
}

//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 1;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addcloud(x, y, scale, linesize);       // Main cloud
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addmoon(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  }
  else
  {
    addcloud(x, y, scale, linesize);       // Main cloud
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  }
}


//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  y += 10;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x + 30, y - 35, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
  }
  else
  {
    addcloud(x + 30, y - 35, 5, linesize); // Cloud top right
    addcloud(x - 20, y - 25, 7, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);       // Main cloud
  }
}


//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, IconSize);

  }
  else
  {
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, IconSize);
  }
}


//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, IconSize);
  }
  else
  {
    addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
    addcloud(x, y, scale, linesize);
    addrain(x, y, scale, IconSize);
  }
}


//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y, scale, linesize);
    addtstorm(x, y, scale);
  }
  else
  {
    addcloud(x, y, scale, linesize);
    addtstorm(x, y, scale);
  }
}


//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y, scale, linesize);
    addsnow(x, y, scale, IconSize);
  }
  else
  {
    addcloud(x, y, scale, linesize);
    addsnow(x, y, scale, IconSize);
  }
}


//#########################################################################################
void ChanceSnow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y, scale, linesize);
    addsnow(x, y, scale, IconSize);
  }
  else
  {
    addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
    addcloud(x, y, scale, linesize);
    addsnow(x, y, scale, IconSize);
  }
}


//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {
    addmoon(x, y, moon, IconSize);
    addcloud(x, y - 5, scale, linesize);
    addfog(x, y - 5, scale, linesize, IconSize);
  }
  else
  {
    addcloud(x, y - 5, scale, linesize);
    addfog(x, y - 5, scale, linesize, IconSize);
  }
}


//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;

  if (timeClient.getHours() >= 18 || timeClient.getHours() <= 5)
  {

    addmoon(x, y, moon, IconSize);
    addfog(x, y - 5, scale * 1.4, linesize, IconSize);
  }
  else
  {
    addsun(x, y - 5, scale * 1.4, IconSize);
    addfog(x, y - 5, scale * 1.4, linesize, IconSize);
  }
}


//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, " ? ", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}


//DEVICE STATUS//
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  voltage = voltage * 100;
  //Serial.println("Voltage = " + String(voltage));
  //percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
  percentage = map(voltage, 360, 430, 0, 100);
  if (percentage > 100)
  {
    percentage = 100;
  }
  if (percentage < 0)
  {
    percentage = 0;
  }
  //if (voltage >= 4.20) percentage = 100;
  //if (voltage <= 3.50) percentage = 0;
  display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
  display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
  display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
  drawString(x + 70, y - 11, String(percentage) + " % ", RIGHT);
}


void setup() {
  //Serial.begin(115200);
  InitialiseDisplay();
  WiFi.mode(WIFI_STA);
  //wifiManager.resetSettings();
  wifiManager.setConfigPortalTimeout(120);
  WiFi.begin();
  timeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    //Serial.print(".");
    if (millis() - timeout > 35000)
    {
      res = wifiManager.autoConnect("LineaMeteoStazioneVisual", "LaMeteo2005");

      if (!res) {
        //Serial.println("Failed to connect");
        esp_sleep_enable_timer_wakeup(60 * 1000000LL);
        esp_deep_sleep_start();
      }
    }
  }
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);   // connect to firebase
  Firebase.reconnectWiFi(true);
  Firebase.setMaxRetry(Weather, 2);
  getDataTime();
  Wire.begin();
  bme.begin();
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  //bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms
}

void loop() {
  getValues();
  DisplayWeather();
  display.display(false); // Full screen update mode
  delay(100);
  BeginSleep();
}
