#include <NTPClient.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include "FS.h"

#define DHT_PIN D5
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define WIFI_SSID "BN-Home-New" 
#define WIFI_PASS "123454321"
WiFiClientSecure espClient;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
long lastMsg = 0;

#define MQTT_TOPIC "envData"
#define AWS_HOST "awyquncbsibad-ats.iot.ap-south-1.amazonaws.com"

void callback(char *topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }  
  Serial.println();
}

PubSubClient client(AWS_HOST, 8883, callback, espClient);

void setup_wifi() {
  delay(10);
  espClient.setBufferSizes(512,512);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println('Wifi Connected with IP address: '); Serial.print(WiFi.localIP());

  timeClient.begin();
  while(!timeClient.update()){
    timeClient.forceUpdate(); 
  }
  espClient.setX509Time(timeClient.getEpochTime());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if(client.connect("ESP8266")) {
      Serial.println("connected");
      client.subscribe("envData");  
    } else {
      Serial.print("Failed: "); Serial.print(client.state());
      char buf[256];
      espClient.getLastSSLError(buf, 256);
      Serial.print("WiFi SSL Error: ");Serial.print(buf);
      delay(5000);
    }
  }  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  dht.begin();
  setup_wifi();
  delay(1000);
  
  if(!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;  
  }

  Serial.print("Heap: "); Serial.print(ESP.getFreeHeap());
  
  File cert = SPIFFS.open("/cert.der", "r");
  if(!cert)
    Serial.println("failed to open cert file");
  delay(1000);
  if(espClient.loadCertificate(cert))
    Serial.println("cert loaded");

  File priv_key = SPIFFS.open("/private.der", "r");
  if(!priv_key)
    Serial.println("failed to open priv key file");
  delay(1000);
  if(espClient.loadPrivateKey(priv_key))
    Serial.println("private key loaded");

  File ca = SPIFFS.open("/ca.der", "r");
  if(!ca)
    Serial.println("failed to open CA file");
  delay(1000);
  if(espClient.loadCACert(ca))
    Serial.println("CA loaded");

  Serial.print("Heap: "); Serial.print(ESP.getFreeHeap());
}

void loop() {
  // put your main code here, to run repeatedly:
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    String ts = timeClient.getFormattedDate();
    if(isnan(h) || isnan(t)) {
      Serial.println("failed to read data from DHT sensor");
      return;
    }
    String temp = "{\"t\":"+ String(t) +",\"h\":"+ String(h) +",\"ts\":\""+ ts +"\"}";
    int len = temp.length();
    char msg[len+1];
    temp.toCharArray(msg, len+1);
    
    Serial.print(msg);
    client.publish("envData", msg);
  }
  delay(300000);

}
