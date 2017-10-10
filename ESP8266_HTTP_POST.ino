#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <stdlib.h>
#include <DHT.h>

#define your_ssid "wifi_network_name" // WiFi network name
#define your_password "your_password" // WiFi password
#define your_host "host_ip" // local host that the http server accesses to
#define your_device_id "your_device_id" //your device ID (or MAC address)
#define your_api_key "your_api_key" //your api key 
#define sleepTimeS 60  //the amount of time you want your esp to sleep in seconds. NOTE: GPIO 16 NEEDS TO BE CONNECTED TO RST PIN FOR ESP TO WAKE UP.
#define http_publish_addrss "http_server_publishing_address" //the http address that you want to publish to

extern "C"{
  #include "user_interface.h"
} //used to read internal battery voltage
ADC_MODE(ADC_VCC); // indicates that pin no A0 is in the Read VCC mode. NOTE: pin A0 cannot be used elsewhere due to it being set in this mode

unsigned int localPort = 2390; 
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";
//const char* ntpServerName = "1.in.pool.ntp.org";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
#define DHTPIN 2 //PIN NO 2 IS CONNECTED TO THE DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
int pin_light = D5; //PIN NO 5 IS CONNECTED TO LDR
StaticJsonBuffer<200> jsonBuffer; //CREATE A JSON BUFFER
StaticJsonBuffer<200> sensorBuffer; // create a json buffer
String jsonString ; //string to store the json to be sent to server
String sensorString ; // string to store the data from sensors
String  sensor_light; 
String pubString;
String epoch_s;
char key;
float vdd;
float thou; 

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    // wait serial port initialization
  }
  Serial.print("Connecting to ");
  Serial.println(your_ssid);

  WiFi.begin(your_ssid, your_password);
    
  while (WiFi.status() != WL_CONNECTED) //wifi connected
  {
    delay(500);
    Serial.print(".");
  }
   pinMode(DHTPIN, INPUT); //declare dht as an input
   pinMode(pin_light, INPUT); // declare ldr as an input
   dht.begin();
   Serial.println("");
   Serial.println("WiFi connected");
   Serial.println("IP address: ");
   Serial.println(WiFi.localIP()); // print ip address of the network you are on
   Serial.println("Starting UDP");
   udp.begin(localPort); // begin udp connection 
   Serial.print("Local port: ");
   Serial.println(udp.localPort()); //print port used for udp
   delay(2000);

  while(1){
   
   udp_connection(); 
   delay(2000);
}
}

void udp_connection(){
   WiFi.hostByName(ntpServerName, timeServerIP); 

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  
  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
   
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer


    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
   
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;

    unsigned int main = epoch;
    
    int leap_days=0; 
    int leap_year_ind=0;
  
    // Add or substract time zone here. 
    // epoch =  epoch + 19800 ; //GMT +5:30 = +19800 seconds for india
    epoch_s = epoch; // store epoch in a string
    Serial.println(epoch_s); 
    POST(); // call post function
}
  delay(2000); // wait
}


unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
 
}


void POST(){

int light = digitalRead(pin_light);
  if (light == 1){
      sensor_light = "ON"; // if digital state is HIGH then the room is said to be lit
     }
     else{
      sensor_light = "OFF"; // if digital state is LOW then the room is said to be not lit

     }

  float h = dht.readHumidity(); // read humidity of DHT22
  thou = float(1000);
  vdd = system_get_vdd33()/thou; // read internal voltage of ESP8266 and divide it by 1000 to get a decimal value
  float t = dht.readTemperature(); // read temperature of DHT22

  JsonObject& root = jsonBuffer.createObject(); // create json object root

  root["resourceID"]=your_device_id;
  
  JsonObject& nested = sensorBuffer.createObject();
  nested["epoch"] = epoch_s;
  nested["temperature"] = t;
  nested["humidity"] = h;
  nested["light_state"] = sensor_light;
  nested["battery_voltage"] = vdd;

  nested.printTo(sensorString); // put object "nested" in buffer sensorString

  root["data"] = sensorString; // store sensorString in "data" of object root
  
  root.printTo(jsonString); // put root in jsonString 

  Serial.println(jsonString); // print jsonString on serial port for reference

 if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Requesting POST: ");
    HTTPClient http;
 
    http.begin(http_publish_address);

    http.addHeader("User-Agent", "Arduino/1.0");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", your_api_key);
    int httpCode = http.POST(jsonString);
    String payload = http.getString();
    Serial.println(payload); //printing the server response
    http.end(); //CLOSE HTTP CONNECTION
   
    Serial.println("closing connection");
    delay(3000);
  }
  else
  {
    Serial.println("connection failed");
    return;
  }
     Serial.println("ESP8266 in sleep mode");
   ESP.deepSleep(sleepTimeS * 1000000); // put esp in sleep mode 
}
void loop() {
  // not used in this example as ESP is put to sleep and loop does not execute when sleep function is active
}
