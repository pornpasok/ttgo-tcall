#include <PMserial.h>
SerialPM pms(PMSx003, 21, 22);  // PMSx003, RX, TX

#include <WiFi.h>
#include <HTTPClient.h>

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  59        /* Time ESP will go to sleep (in seconds) */

const char* ssid     = "SookYenFarm";    // SSID Wifi
const char* password = "WIFI-PASSWORD";   // Password Wifi
const char* host = "http://appname.herokuapp.com/esp-post-data.php";
const char* api   = "API-Key";  //API Key
String sensorLocation = "12.7581423,102.1468503";

long timeout; // Time to WiFi Connect
// RSSI (WiFi Signal)
long rssi;

uint32_t chipId = 0;
//float vbatt = analogRead(35)*2/1135.0f;
// TTGO SIM800C AXP192
float vbatt = analogRead(35)/1000.0f;

int LED_BUILTIN = 12;

void connect() {
   // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    // LED Status
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    //delay(500);
    Serial.print(".");
    timeout = millis();
    Serial.println(timeout);

    // Time Out got deep_sleep_mode save battery
    if (timeout > 30000) {
      Serial.print("TIME_OUT: ");
      Serial.println(timeout);
      Serial.print("Sleeping ");
      Serial.print(TIME_TO_SLEEP);
      Serial.println(" seconds ..");
      // Deep Sleep TIME_TO_SLEEP seconds
      ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("--------------------");
  
  pms.read();                   // read the PM sensor
  Serial.print(F("PM1.0 "));Serial.print(pms.pm01);Serial.print(F(", "));
  Serial.print(F("PM2.5 "));Serial.print(pms.pm25);Serial.print(F(", "));
  Serial.print(F("PM10 ")) ;Serial.print(pms.pm10);Serial.println(F(" [ug/m3]"));

  // Chip ID
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  Serial.print("Chip ID: ");
  Serial.println(chipId);
  
  // VCC
  Serial.print("VBATT: ");
  Serial.println(vbatt);

  // Wi-Fi Signal
  rssi = WiFi.RSSI();
  Serial.print("WiFi Signal: ");
  Serial.println(rssi);
  Serial.println("--------------------");
  
  Serial.print("connecting to ");
  Serial.println(host);

  // Show Connect Status
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  //delay(1000);
  Serial.println("LED ON");

  HTTPClient http;
  http.begin(host);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // We now create a URI for the request
  String httpRequestData = "api_key=";
  httpRequestData += api;
  httpRequestData += "&sensor=";
  httpRequestData += chipId;
  httpRequestData += "&location=";  
  httpRequestData += sensorLocation;
  httpRequestData += "&value1=";  
  httpRequestData += pms.pm01;
  httpRequestData += "&value2=";  
  httpRequestData += pms.pm25;
  httpRequestData += "&value3=";  
  httpRequestData += pms.pm10;
  httpRequestData += "&value4=";  
  httpRequestData += rssi;
  httpRequestData += "&value5=";  
  httpRequestData += vbatt;
  
  Serial.print("httpRequestData: ");
  Serial.println(httpRequestData);

  // Send HTTP POST request
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
  digitalWrite(LED_BUILTIN, HIGH);
  //delay(1000);
  Serial.println("LED OFF");
}

void setup(void) {
  Serial.begin(115200);
  Serial.setTimeout(2000);
  // LED Status
  pinMode(LED_BUILTIN, OUTPUT);
  
  delay(10);
  pms.init();                   // config serial port
  
  connect();
  
  Serial.print("Sleeping ");
  Serial.print(TIME_TO_SLEEP);
  Serial.println(" seconds ..");
  // Deep Sleep TIME_TO_SLEEP seconds
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR); // 60e6 is 60 microsecondsESP.
}

void loop() {
}
