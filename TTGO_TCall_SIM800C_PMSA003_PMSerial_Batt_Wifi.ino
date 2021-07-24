#include <PMserial.h>
SerialPM pms(PMSx003, 18, 19);  // PMSx003, RX, TX

#include <WiFi.h>
#include <HTTPClient.h>
#include <axp20x.h>         //https://github.com/lewisxhe/AXP202X_Library
AXP20X_Class axp;

#define I2C_SDA              21
#define I2C_SCL              22

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  59        /* Time ESP will go to sleep (in seconds) */

const char* ssid     = "SookYenFarm";    // SSID Wifi
const char* password = "WIFI-PASSWORD";   // Password Wifi
const char* host = "http://tonofarm.herokuapp.com/esp-post-data.php";
const char* api   = "API-Key";  //API Key
String sensorLocation = "12.7581423,102.1468503";

long timeout; // Time to WiFi Connect
// RSSI (WiFi Signal)
long rssi;

uint32_t chipId = 0;
//float vbatt = analogRead(35)*2/1135.0f;

int LED_BUILTIN = 12;

// Read Volt TTGO SIM800C AXP192
bool setupPMU()
{
// For more information about the use of AXP192, please refer to AXP202X_Library https://github.com/lewisxhe/AXP202X_Library
    Wire.begin(I2C_SDA, I2C_SCL);
    int ret = axp.begin(Wire, AXP192_SLAVE_ADDRESS);

    if (ret == AXP_FAIL) {
        Serial.println("AXP Power begin failed");
        return false;
    }

    //! Turn off unused power
    axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);
    axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);
    axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);
    axp.setPowerOutPut(AXP192_DCDC2, AXP202_OFF);
    axp.setPowerOutPut(AXP192_EXTEN, AXP202_OFF);

    //! Do not turn off DC3, it is powered by esp32
    // axp.setPowerOutPut(AXP192_DCDC3, AXP202_ON);

    // Set the charging indicator to turn off
    // Turn it off to save current consumption
    // axp.setChgLEDMode(AXP20X_LED_OFF);

    // Set the charging indicator to flash once per second
    // axp.setChgLEDMode(AXP20X_LED_BLINK_1HZ);


    //! Use axp192 adc get voltage info
    axp.adc1Enable(AXP202_VBUS_VOL_ADC1 | AXP202_VBUS_CUR_ADC1 | AXP202_BATT_CUR_ADC1 | AXP202_BATT_VOL_ADC1, true);

    float vbus_v = axp.getVbusVoltage();
    float vbus_c = axp.getVbusCurrent();
    float batt_v = axp.getBattVoltage();
    // axp.getBattPercentage();   // axp192 is not support percentage
    Serial.printf("VBUS:%.2f mV %.2f mA ,BATTERY: %.2f\n", vbus_v, vbus_c, batt_v);

    return true;
}

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

  // TTGO SIM800C AXP192
  float vbus_v = axp.getVbusVoltage();
  float vbus_c = axp.getVbusCurrent();
  float batt_v = axp.getBattVoltage();
    
  float vbatt = batt_v/1000.0f;
  float vcharge = vbus_v/1000.0f;
  
  Serial.print("vbus_v: ");
  Serial.print(vbus_v);
  Serial.print(" vbus_c: ");
  Serial.print(vbus_c);
  Serial.print(" batt_v: ");
  Serial.println(batt_v);
  
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
  httpRequestData += "&value6=";  
  httpRequestData += vcharge;
  
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

  // Read Volt from AXP192
  setupPMU();
  
  connect();
  
  Serial.print("Sleeping ");
  Serial.print(TIME_TO_SLEEP);
  Serial.println(" seconds ..");
  // Deep Sleep TIME_TO_SLEEP seconds
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR); // 60e6 is 60 microsecondsESP.
}

void loop() {
}
