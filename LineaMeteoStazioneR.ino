[code]
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///****************************************LINEAMETEO STAZIONE RECEIVER********************************************///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////LIBRARY DECLARATION////////////////////////////////////////
//#include <WiFi.h>
//#include <FirebaseESP32.h>
#include "FirebaseESP8266.h"
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <String.h>
#include <Wire.h>
#include "NTPClient.h"
#include <WiFiUDP.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp;
WiFiServer server(80);
WiFiClient client;
WiFiManager wifiManager;


////////////////////*********FIREBASE DETAILS************///////////////////////////////////
#define FIREBASE_HOST ""                 // the project name address from firebase id
#define FIREBASE_AUTH ""            // the secret key generated from firebase
FirebaseData Weather;


//////////////////////*********NTP SERVER************//////////////////////////////////////
// Define NTP properties
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS "pool.ntp.org" // "ca.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)
// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS , NTP_OFFSET, NTP_INTERVAL);


////////////////////*********THINGSPEAK DETAILS************////////////////////////////////
//#include <HTTPClient.h>
//HTTPClient http;
String myWriteAPIKey;
const char* serverThingSpeak = "api.thingspeak.com";
//String host = "https://api.thingspeak.com";
//String url;


////////////////////*********WEATHERCLOUD************////////////////////////////////
String Weathercloud_ID;
String Weathercloud_KEY;
const int httpPort = 80;
const char* Weathercloud = "http://api.weathercloud.net";


////////////////////*********WUNDERGROUND************////////////////////////////////
char serverWU [] = "rtupdate.wunderground.com";
char WEBPAGE [] = "GET /weatherstation/updateweatherstation.php?";
String ID;
String Key;


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
float tempSHT1x;
float tempinside;
//TEMPERATURE EXTREMES//
float maxTemp;
float minTemp;
float maxTempSleep;
float minTempSleep;


//////////HUMIDITY/////////////
int humiditySHT1x;
int humidity;
int humidityinside;
//HUMIDITY EXTREMES//
int maxHumidity;
int minHumidity;

//////////PRESSURE/////////////
float pressurehpa;
int CALIBRATION;


////////////WIND///////////////
float WindSpeed;
int CalDirection;
float Gust;


////////////RAIN///////////////
float rainrate;
float rainrateMax;
float mmPioggia;


////////////DEWPOINT///////////////
float dewPoint; // variabile del punto di rugiada
float dewPointIN;
//DEW POINT EXTREMES//
//float dewPointmax;
//float dewPointmin;


////////////HEATINDEX///////////////
float heatIndex; // variabile dell'indice di calore
float heatIndexIN;
//float heatIndexMax;


////////////WINDCHILL///////////////
float windchill; // variabile del raffreddamento causato dal vento
//float Windchillmin;


/////////////UVINDEX////////////////
int UVindex;
//int UVmax;


////TIMING AND OTHER VARIALABLES////
unsigned long uploadtime = 0;
unsigned long timeout;
bool res;
unsigned long previous = 0;
unsigned long previousrecord;
const unsigned long comparesensortime = 600000;
byte CurrentDay;
byte Sleep;
String Language;
boolean readvalues = false;
byte RESETDATA = 0;
int TIMEZONE;
long TIMEZONEINSECONDS;


//////////////////////////////////////////////////////
//////////////FUNCTIONS DECLARATIONS//////////////////
/////////////////////////////////////////////////////
//**************************************************//
/////////READ FROM DATABASE AND WRITE DATA////////////
//**************************************************//


void readData()
{

  if (millis() - previous >= uploadtime)
  {
    previous = millis();
    uploadtime = 130000;
    readvalues = true;
    float pressure = bmp.readPressure() + CALIBRATION;
    pressurehpa = pressure / 100;

    if (Firebase.getFloat(Weather, "SHT3x/Temperature/Temperature"))
    {
      temp = Weather.floatData();
    }

    if (Firebase.getInt(Weather, "SHT3x/Humidity"))
    {
      humidity = Weather.intData();
    }

    if (Firebase.getFloat(Weather, "SHT1x/Temperature"))
    {
      tempSHT1x = Weather.floatData();
    }

    if (Firebase.getInt(Weather, "SHT1x/Humidity"))
    {
      humiditySHT1x = Weather.intData();
    }

    if (Firebase.getFloat(Weather, "Rain/RainRate"))
    {
      rainrate = Weather.floatData();
    }

    if (Firebase.getFloat(Weather, "Wind/WindSpeed"))
    {
      WindSpeed = Weather.floatData();
    }

    /*if (Firebase.getFloat(Weather, "Pressure/Pressure"))
    {
      pressurehpa = Weather.floatData();
    }*/

    if (Firebase.getFloat(Weather, "Inside/Temperature"))
    {
      tempinside = Weather.floatData();
    }

    if (Firebase.getInt(Weather, "Inside/Humidity"))
    {
      humidityinside = Weather.intData();
    }

    if (Firebase.getInt(Weather, "Wind/WindDirection"))
    {
      CalDirection = Weather.intData();
    }

    if (Firebase.getInt(Weather, "/UVindex"))
    {
      UVindex = Weather.intData();
    }
  }
}

void writeData()
{
  if (readvalues == true)
  {
    readvalues = false;

    dewPoint = (temp - (14.55 + 0.114 * temp) * (1 - (0.01 * humidity)) - pow(((2.5 + 0.007 * temp) * (1 - (0.01 * humidity))), 3) - (15.9 + 0.117 * temp) * pow((1 - (0.01 * humidity)), 14));  //equazione per il calcolo del dewpoint
    //INSIDE//
    dewPointIN = (tempinside - (14.55 + 0.114 * tempinside) * (1 - (0.01 * humidityinside)) - pow(((2.5 + 0.007 * tempinside) * (1 - (0.01 * humidityinside))), 3) - (15.9 + 0.117 * tempinside) * pow((1 - (0.01 * humidityinside)), 14));  //equazione per il calcolo del dewpoint


    if (temp < 11)
    {
      windchill = (13.12 + 0.6215 * temp) - (11.37 * pow(WindSpeed, 0.16)) + (0.3965 * temp * pow(WindSpeed, 0.16));//equazione per il calcolo del windchill
    }
    else
    {
      windchill = temp;
    }

    if (temp > 21)
    {
      heatIndex = c1 + (c2 * temp) + (c3 * humidity) + (c4 * temp * humidity) + (c5 * sq(temp)) + (c6 * sq(humidity)) + (c7 * sq(temp) * humidity) + (c8 * temp * sq(humidity)) + (c9 * sq(temp) * sq(humidity));
    }
    else if (temp <= 21)
    {
      heatIndex = temp;
    }

    //INSIDE//
    if (tempinside > 21)
    {
      heatIndexIN = c1 + (c2 * tempinside) + (c3 * humidityinside) + (c4 * tempinside * humidityinside) + (c5 * sq(tempinside)) + (c6 * sq(humidityinside)) + (c7 * sq(tempinside) * humidityinside) + (c8 * tempinside * sq(humidityinside)) + (c9 * sq(tempinside) * sq(humidityinside));
    }
    else if (tempinside <= 21)
    {
      heatIndexIN = tempinside;
    }

    maxminvalues();
    push_to_weathercloud();
    wunderground();
    THINGSPEAK();
  }
}


////////////////DAYLY EXTREMES/////////////////////////
void maxminvalues()
{
  if (humidity > maxHumidity)
  {
    maxHumidity = humidity;
    Firebase.setInt(Weather, "SHT3x/HumidityMax", maxHumidity);
  }

  if (humidity < minHumidity)
  {
    minHumidity = humidity;
    Firebase.setInt(Weather, "SHT3x/HumidityMin", minHumidity);
  }

  if (Sleep == 1)
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

  if (Sleep == 0)
  {
    if ((temp > maxTempSleep) && (temp != 0))
    {
      maxTempSleep = temp;
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMax", maxTempSleep);
    }

    if ((temp < minTempSleep) && (temp != 0))

    {
      minTempSleep = temp;
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMin", minTempSleep);
    }
  }
}


//**************************************************//
//////////////////DISPLAY DATA////////////////////////
//**************************************************//


///////////////////GET TIME//////////////////////////
void gettime()
{
  TIMEZONEINSECONDS = TIMEZONE * 3600;
  timeClient.update();
  timeClient.setTimeOffset(TIMEZONEINSECONDS);
  timeClient.getFullFormattedTime();

  if (CurrentDay != timeClient.getDate())
  {
    CurrentDay = timeClient.getDate();
    maxHumidity = humidity;
    minHumidity = humidity;
    Firebase.setInt(Weather, "ChangeTime/CurrentDay", CurrentDay);
    Firebase.setInt(Weather, "SHT3x/HumidityMax", maxHumidity);
    Firebase.setInt(Weather, "SHT3x/HumidityMin", minHumidity);
    //Serial.println("Data reset at midnight");
    if (Sleep == 0)
    {
      maxTempSleep = temp;
      minTempSleep = temp;
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMax", maxTempSleep);
      Firebase.setFloat(Weather, "SHT3x/Temperature/TemperatureMin", minTempSleep);
    }
    if (Firebase.getInt(Weather, "ChangeTime/TIMEZONE"))
    {
      TIMEZONE = Weather.intData();
    }
    //Serial.println("TIMEZONE CHECKED");
  }
}


//*****************************************************//
//////////////WEBSERVER AND HOST FOR DATA////////////////
//*****************************************************//

void THINGSPEAK()
{
  if (millis() - previousrecord >= comparesensortime)
  {
    previousrecord = millis();
    /*url = "/update?api_key=" + myWriteAPIKey + "&field1=" + (String)(temp) + "&field2=" + (String)(humidity) + "&field3=" + (String)(tempSHT1x) + "&field4=" + (String)(humiditySHT1x) + "&field5=" + (String)(WindSpeed) + "&field6=" + (String)(Gust);
      Serial.println(host + url);
      http.begin(host + url);
      if (http.GET() == HTTP_CODE_OK) Serial.println(http.getString());
      http.end();*/
    WiFiClient client;
    if (client.connect(serverThingSpeak, 80)) { // use ip 184.106.153.149 or api.thingspeak.com
      //Serial.print("connected to ");
      //Serial.println(client.remoteIP());

      String postStr = myWriteAPIKey;
      postStr += "&field1=";
      postStr += String(temp);
      postStr += "&field2=";
      postStr += String(humidity);
      postStr += "&field3=";
      postStr += String(tempSHT1x);
      postStr += "&field4=";
      postStr += String(humiditySHT1x);
      postStr += "&field5=";
      postStr += String(WindSpeed);
      postStr += "&field6=";
      postStr += String(Gust);
      /*postStr += "&field7=";
        postStr += String(getWindDirectionMax());
        postStr += "&field8=";
        postStr += String(quality);
        postStr += "\r\n\r\n";*/

      client.print("POST /update HTTP/1.1\n");
      client.print("Host: api.thingspeak.com\n");
      client.print("Connection: close\n");
      client.print("X-THINGSPEAKAPIKEY: " + myWriteAPIKey + "\n");
      client.print("Content-Type: application/x-www-form-urlencoded\n");
      client.print("Content-Length: ");
      client.print(postStr.length());
      client.print("\n\n");
      client.print(postStr);
      delay(50);
    }
    client.stop();
  }
}

void lineameteo()
{
  WiFiClient client = server.available();

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.print(timeClient.getFullFormattedTime());
  client.print("&v0=-37.938906&v0=145.129615&v0=Clayton South(VIC)&v0=52m s.l.m.&v0=");
  client.print(temp, 1);
  client.print("&v0=");
  client.print(humidity);
  client.print("&v0=");
  client.print(pressurehpa, 1);
  client.print("&v0=");
  client.print(WindSpeed, 1);
  client.print("&v0=");
  if (CalDirection < 22)
    client.print("N");
  else if (CalDirection < 67)
    client.print("NE");
  else if (CalDirection < 112)
    client.print("E");
  else if (CalDirection < 157)
    client.print("SE");
  else if (CalDirection < 212)
    client.print("S");
  else if (CalDirection < 247)
    client.print("SW");
  else if (CalDirection < 292)
    client.print("W");
  else if (CalDirection < 337)
    client.print("NW");
  else
    client.print("N");
  client.print("&v0=");
  client.print(mmPioggia, 1);
  client.print("&v0=");
  client.print(minTempSleep, 1);
  client.print("&v0=");
  client.print(maxTempSleep, 1);
  client.print("&v0=");
  client.print(Gust, 1);
  client.print("&v0=");
  client.print(rainrate, 1);
  client.println("</html>");

}

void push_to_weathercloud()
{
  //Serial.println("Initializing data push to Weathercloud.");

  if (!client.connect(Weathercloud, httpPort)) {
    //Serial.println("Connecting to Weatherloud failed.\n");
    return;
  }
  client.print("GET /set");
  client.print("/wid/"); client.print(Weathercloud_ID);
  client.print("/key/"); client.print(Weathercloud_KEY);

  client.print("/temp/"); client.print(temp * 10);
  client.print("/tempin/"); client.print(tempinside * 10);
  client.print("/humin/"); client.print(humidityinside);
  client.print("/chill/"); client.print(windchill * 10);
  client.print("/dew/"); client.print(dewPoint * 10);
  client.print("/dewin/"); client.print(dewPointIN * 10);
  client.print("/heat/"); client.print(heatIndex * 10);
  client.print("/heatin/"); client.print(heatIndexIN * 10);
  client.print("/hum/"); client.print(humidity);
  client.print("/wspd/"); client.print(WindSpeed * 10);
  client.print("/wdir/"); client.print(CalDirection);
  client.print("/bar/"); client.print(pressurehpa * 10);
  client.print("/rain/"); client.print(mmPioggia * 10);
  client.print("/rainrate/"); client.print(rainrate * 10);
  client.print("/uvi/"); client.print(UVindex * 10);
  //client.print("/solarrad/"); client.print(0);

  client.println("/ HTTP/1.1");
  client.println("Host: api.weathercloud.net");
  client.println("Connection: close");
  client.println();
  //Serial.println("Data pushed to Weathercloud sucessfuly.\n");;
}

void wunderground()
{
  //Serial.print("Connecting to ");
  //Serial.println(serverWU);
  WiFiClient client;
  if (client.connect(serverWU, 80)) {
    //Serial.print("connected to ");
    //Serial.println(client.remoteIP());
    delay(100);
  } else {
    //Serial.println("connection failed");
  }
  client.print(WEBPAGE);
  client.print("ID=");
  client.print(ID);
  client.print("&PASSWORD=");
  client.print(Key);
  client.print("&dateutc=now&winddir=");
  client.print(CalDirection);
  client.print("&tempf=");
  client.print(temp * 1.8 + 32);
  client.print("&windspeedmph=");
  client.print(WindSpeed / 1.609);
  client.print("&windgustmph=");
  client.print(Gust / 1.609);
  client.print("&dewptf=");
  client.print(dewPoint * 1.8 + 32);
  client.print("&humidity=");
  client.print(humidity);
  client.print("&baromin=");
  client.print(pressurehpa * 0.02953);
  client.print("&rainin=");
  client.print(rainrate / 25.4);
  client.print("&dailyrainin=");
  client.print(mmPioggia / 25.4);
  client.print("&UV=");
  client.print(UVindex);
  client.print("&softwaretype=Linea-Meteo&action=updateraw&realtime=1&rtfreq=30");
  client.print("/ HTTP/1.1\r\nHost: rtupdate.wunderground.com:80\r\nConnection: close\r\n\r\n");
  //Serial.println(" ");
  delay(50);
}


//**************************************************//
////////////////SETUP FUNCTIONS///////////////////////
//**************************************************//

void SetupData()
{
  Firebase.setString(Weather, "Connection/IPAddress", WiFi.localIP().toString());

  if (Firebase.getInt(Weather, "ChangeTime/RESETDATA"))
  {
    RESETDATA = Weather.intData();
  }
  if (RESETDATA == 0)
  {
    Firebase.setInt(Weather, "Sleep", 1);
    Firebase.setInt(Weather, "Wind/Offset", 0);
    Firebase.setFloat(Weather, "Rain/mmGoccia", 0.2);
    Firebase.setInt(Weather, "ChangeTime/TIMEZONE", 1);
    Firebase.setInt(Weather, "ChangeTime/RESETDATA", 1);
    Firebase.setInt(Weather, "ChangeTime/SendDataTime", 90);
    Firebase.setInt(Weather, "Pressure/Calibration", 0);
    Firebase.setInt(Weather, "SHT3x/HumidityMax", -100);
    Firebase.setInt(Weather, "SHT3x/HumidityMin", 100);
    Firebase.setInt(Weather, "SHT3x/Temperature/TemperatureMax", -100);
    Firebase.setInt(Weather, "SHT3x/Temperature/TemperatureMin", 100);
    Firebase.setString(Weather, "Services/Wunderground/ID", "YourID");
    Firebase.setString(Weather, "Services/Wunderground/Key", "YourKey");
    Firebase.setString(Weather, "Services/WeatherCloud/ID", "YourID");
    Firebase.setString(Weather, "Services/WeatherCloud/Key", "YourKey");
    Firebase.setString(Weather, "Services/ThingSpeak/myWriteAPIKey", "WriteKeyAPI");
    Firebase.setString(Weather, "Services/OpenWeather/API", "API");
    Firebase.setString(Weather, "Services/OpenWeather/Hemisphere", "north or south");
    Firebase.setString(Weather, "Services/OpenWeather/Language", "en");
    Firebase.setString(Weather, "Services/OpenWeather/Latitude", "0.0000");
    Firebase.setString(Weather, "Services/OpenWeather/Longitude", "0.0000");
  }
}

void getDataTime()
{
  //Serial.println("Getting Time...");
  if (Firebase.getInt(Weather, "Pressure/Calibration"))
  {
    CALIBRATION = Weather.intData();
  }

  if (Firebase.getInt(Weather, "ChangeTime/CurrentDay"))
  {
    CurrentDay = Weather.intData();
  }

  if (Firebase.getInt(Weather, "ChangeTime/TIMEZONE"))
  {
    TIMEZONE = Weather.intData();
  }

  if (Firebase.getInt(Weather, "Sleep"))
  {
    Sleep = Weather.intData();
  }

  if (Firebase.getInt(Weather, "SHT3x/HumidityMax"))
  {
    maxHumidity = Weather.intData();
  }

  if (Firebase.getInt(Weather, "SHT3x/HumidityMin"))
  {
    minHumidity = Weather.intData();
  }

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

  if (Firebase.getString(Weather, "Services/Wunderground/ID"))
  {
    ID = Weather.stringData();
  }

  if (Firebase.getString(Weather, "Services/Wunderground/Key"))
  {
    Key = Weather.stringData();
  }

  if (Firebase.getString(Weather, "Services/WeatherCloud/ID"))
  {
    Weathercloud_ID = Weather.stringData();
  }

  if (Firebase.getString(Weather, "Services/WeatherCloud/Key"))
  {
    Weathercloud_KEY = Weather.stringData();
  }

  if (Firebase.getString(Weather, "Services/ThingSpeak/myWriteAPIKey"))
  {
    myWriteAPIKey = Weather.stringData();
  }
}


void setup()
{
  //Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  //wifiManager.resetSettings();
  wifiManager.setConfigPortalTimeout(300);
  WiFi.begin();
  timeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    //Serial.print(".");
    if (millis() - timeout > 35000)
    {
      res = wifiManager.autoConnect("LineaMeteoStazioneR", "LaMeteo2005");

      if (!res) {
        //Serial.println("Failed to connect");
        //ESP.deepSleep(15 * 1000000, WAKE_RF_DEFAULT);
        ESP.restart();
      }
    }
  }
  timeClient.begin();   // Start the NTP UDP client
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);   // connect to firebase
  Firebase.reconnectWiFi(true);
  Firebase.setMaxRetry(Weather, 2);
  //Weather.setBSSLBufferSize(1024, 1024);
  //Set the size of HTTP response buffers in the case where we want to work with large data.
  //Weather.setResponseSize(1024);
  SetupData();
  getDataTime();
  server.begin();
  Wire.begin();
  bmp.begin();
}

void loop()
{
  readData();
  writeData();
  gettime();
  lineameteo();
}
[/code]
