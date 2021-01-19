#include "DHT.h"
#define DHTPIN 15   // NodeMCU PIN D1
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);

#define uS_TO_S_FACTOR 1000000     /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  598        /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour */

uint8_t temprature_sens_read();

float Temp;
float Humidity;
float Pressure;
uint32_t chipId = 0;
float batteryLevel = analogRead(35)*2/1135.0f;
//int soil = 0;
int soil = analogRead(13);


// Your GPRS credentials (leave empty, if not needed)
const char apn[]      = "Internet"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "true"; // GPRS User
const char gprsPass[] = "true"; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 

// Server details
// The server variable can be just a domain name or it can have a subdomain. It depends on the service you are using
// const char* host = "http://tonofarm.herokuapp.com/esp-post-data.php";
const char* server = "tonofarm.herokuapp.com";
const char* resource ="/esp-post-data.php";
String apiKeyValue = "tonofarm.io";  //API Key
String sensorLocation = "12.7581423,102.1468503";

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial
// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
//#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

// TinyGSM Client for Internet connection
TinyGsmClient client(modem);

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}

void setup() {
  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(115200);

  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);

  // Keep power when running from battery
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  
  // You might need to change the BME280 I2C address, in our case it's 0x76
  //if (!bme.begin(0x76, &I2CBME)) {
  //  Serial.println("Could not find a valid BME280 sensor, check wiring!");
  //  while (1);
  //}

  // Configure the wake up source as timer wake up  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void loop() {
  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" fail");
  }
  else {
    SerialMon.println(" OK");
    
    SerialMon.print("Connecting to ");
    SerialMon.print(server);
    if (!client.connect(server, 80)) {
      SerialMon.println(" fail");
    }
    else {
      SerialMon.println(" OK");

      // Temperature
      dht.begin();
      
      float h = dht.readHumidity();
      float t = dht.readTemperature();
      float f = dht.readTemperature(true);
      if (isnan(h) || isnan(t) || isnan(f)) {
        Serial.println("Failed to read from DHT sensor!");
        // Put ESP32 into deep sleep mode (with timer wake up)
        esp_deep_sleep_start();
        //return;
      }
      float hif = dht.computeHeatIndex(f, h);
      float hic = dht.computeHeatIndex(t, h, false);
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print(" *C ");
      Serial.print(f);
      Serial.print(" *F\t");
      Serial.print("Heat index: ");
      Serial.print(hic);
      Serial.print(" *C ");
      Serial.print(hif);
      Serial.println(" *F");

      // Battery Level
      Serial.print("Battery Level ");
      Serial.println(batteryLevel);
    
      //Temp = ((temprature_sens_read() - 32) / 1.8)-random(20, 30);
      //Humidity = random(10, 100);
      //Pressure = random(1000, 2000);

      Temp = t;
      Humidity = h;

      // Soil Moisture Sensor
      Serial.print("Soil Moisture: ");
      Serial.println(soil);
      
      // Show Modem Signal
      int csq = modem.getSignalQuality();
      SerialMon.print("Modem Signal: ");
      SerialMon.println(csq);

      // ChipID
      for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }

      int csqsignal = map(csq, 0, 31, 0, 100);
      Serial.print("Modem Signal: ");
      Serial.print(csqsignal);
      Serial.println(" %");
    
      String httpRequestData = "api_key=" + apiKeyValue + "&sensor=" + chipId + "&location=" + sensorLocation + "&value1=" + Temp + "&value2=" + Humidity + "&value3=" + csqsignal + "&value4=" + batteryLevel + "&value5=" + soil + "";
      Serial.print("httpRequestData: ");
      Serial.println(httpRequestData);
    
      // Prepare your HTTP POST request data (Temperature in Fahrenheit degrees)
      //String httpRequestData = "api_key=" + apiKeyValue + "&value1=" + String(1.8 * bme.readTemperature() + 32)
      //                       + "&value2=" + String(bme.readHumidity()) + "&value3=" + String(bme.readPressure()/100.0F) + "";
    
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(httpRequestData.length());
      client.println();
      client.println(httpRequestData);

      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 10000L) {
        // Print available data (HTTP response from server)
        /*
        while (client.available()) {
          char c = client.read();
          SerialMon.print(c);
          timeout = millis();
        }
        */
      }
      SerialMon.println();
    
      // Close client and disconnect
      client.stop();
      
      SerialMon.println(F("Server disconnected"));
      modem.gprsDisconnect();
      SerialMon.println(F("GPRS disconnected"));
    }
  }
  // Put ESP32 into deep sleep mode (with timer wake up)
  esp_deep_sleep_start();
  //delay(60000);
  //exit(0);
}
