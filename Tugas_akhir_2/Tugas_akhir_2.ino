// Library untuk mengakses pin analog dan serial
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ---------------Wifi---------------
const char *ssid = "haha";
const char *password = "123456789";
WiFiClient espClient;
PubSubClient client(espClient);

// ---------------MQTT Broker---------------
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

// ---------------Topic---------------

const char *topicpump = "Kelompok2/WaterPump";
const char *topicrelay = "Kelompok2/Relay";
const char *topicping = "Kelompok2/Ping";
const char *topicphmeter = "Kelompok2/Phmeter";

// Deklarasi or smth like that
char msg_phmeter[20];
char msg_pump[20];
char msg_ping[20];
char msg_relay[20];

const int potPin = A0;
float ph;
float voltage = 0;

void setup()
{
    
  Serial.println(F("Program Start"));

  // Inisialisasi serial
  Serial.begin(9600);

  // ---------------Wi-Fi & MQTT---------------

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");
  // connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while(!client.connected()){
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if(client.connect(client_id.c_str(), mqtt_username, mqtt_password)){
      Serial.println("Public emqx mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(1000);
    }
  }

  //subscribe data
  client.subscribe(topic);
  pinMode(TdsSensorPin,INPUT);

}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("------------------------");
}


void setup() {
  Serial.begin(9600);
  pinMode(potPin, INPUT);
  delay(1000);
}

void loop() {
  //For PH Meter
  int value = analogRead(potPin);
  Serial.print(value);
  Serial.print(" | ");
  
  // Convert analog reading to voltage
  voltage = value * (3.3 / 1023.0); // Assuming 5V reference
  
  // Convert voltage to pH using calibration equation
  ph = voltageToPH(voltage); // You need to implement this function
  
  Serial.println(ph, 2); // Print pH with 2 decimal places
  dtostrf(tdsValue,2,2,msg_phmeter);
  client.publish(topictds, msg_phmeter);


  //For Ping
  





  delay(500);
  
}

float voltageToPH(float voltage) {
  // Example calibration equation
  // Replace this with your actual calibration equation
  float pH = 7 - (voltage - 2.5); // This is just an example
  
  return pH;
}