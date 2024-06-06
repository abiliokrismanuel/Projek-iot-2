#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ---------------WiFi---------------
const char *ssid = "Woil";
const char *password = "liow1234";
WiFiClient espClient;
PubSubClient client(espClient);

// ---------------MQTT Broker---------------
const char *mqtt_broker = "46.250.231.231";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = 1883;

// ---------------Topic---------------
const char *topicdhthum = "Kelompok2/dht/humidity";
const char *topicdhttemp = "Kelompok2/dht/temperature";
const char *topictds = "Kelompok2/TDSvalue";
const char *topicwaterflow = "Kelompok2/WaterFlow(FlowRate)";
const char *topicliquid = "Kelompok2/WaterFlow(TotalLiquid)";
const char *topicrelay = "Kelompok2/Relay";

// Deklarasi variabel
char msg_tds[20];
char msg_dhttemp[20];
char msg_dhthum[20];
char msg_waterflowrate[20];
char msg_waterflowliquid[20];
char msg_relay[20];

#define TdsSensorPin A0  // Pin analog untuk sensor TDS
#define VREF 5           // Tegangan referensi ADC (Volt)
#define SCOUNT 30        // Jumlah sampel
int analogBuffer[SCOUNT];  // Array untuk menyimpan nilai analog
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, tdsValue = 0, temperature = 10;

#define DHTPIN 16        // Pin digital untuk DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

int waterPin = 2;        // Pin digital untuk sensor aliran air
volatile long pulse = 0;
unsigned long lastTime = 0;

float volume = 0;
float flowRate = 0;

const int relayPin = 2;

void setup() {
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

  // Menghubungkan ke broker MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public emqx mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(1000);
    }
  }

  // Subscribe topik relay
  client.subscribe(topicrelay);

  pinMode(TdsSensorPin, INPUT);

  pinMode(waterPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(waterPin), increase, RISING);

  pinMode(relayPin, OUTPUT);
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

  // Mengontrol relay berdasarkan pesan yang diterima
  if (String(topic) == "Kelompok2/Relay") {
    if ((char)payload[0] == '1') {
      digitalWrite(relayPin, HIGH);
      Serial.println("Relay ON");
    } else if ((char)payload[0] == '0') {
      digitalWrite(relayPin, LOW);
      Serial.println("Relay OFF");
    }
  }
}

void loop() {
  // Fungsi untuk membaca nilai TDS
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
  }
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 1024.0;
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVolatge = averageVoltage / compensationCoefficient;
    tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5;
    Serial.print("TDS Value:");
    Serial.print(tdsValue, 0);
    Serial.println("ppm");
    dtostrf(tdsValue, 2, 2, msg_tds);
    client.publish(topictds, msg_tds);
  }

  // Fungsi untuk membaca nilai DHT
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  Serial.print("Humidity: ");
  Serial.println(hum);
  Serial.print("Temperature:");
  Serial.print(temp);
  Serial.println("*C");
  dtostrf(hum, 4, 2, msg_dhthum);
  dtostrf(temp, 4, 2, msg_dhttemp);
  client.publish(topicdhttemp, msg_dhttemp);
  client.publish(topicdhthum, msg_dhthum);

  // Fungsi untuk membaca nilai aliran air
  volume = 2.663 * pulse / 1000.0;
  flowRate = volume / ((millis() - lastTime) / 60000.0);

  if (millis() - lastTime > 2000) {
    pulse = 0;
    lastTime = millis();
  }

  Serial.print("Volume: ");
  Serial.print(volume);
  Serial.println(" L/m");
  Serial.print("Flow Rate: ");
  Serial.print(flowRate);
  Serial.println(" L/m");

  dtostrf(volume, 4, 2, msg_waterflowliquid);
  dtostrf(flowRate, 4, 2, msg_waterflowrate);
  client.publish(topicliquid, msg_waterflowliquid);
  client.publish(topicwaterflow, msg_waterflowrate);

  // Pengontrolan relay berdasarkan suhu
  // if (temp >= 35) {
  //   digitalWrite(relayPin, HIGH);
  //   client.publish(topicrelay, "Relay Nyala");
  // } else {
  //   digitalWrite(relayPin, LOW);
  //   client.publish(topicrelay, "Relay Mati");
  // }

  client.loop();
  delay(2000);
}

ICACHE_RAM_ATTR void increase() {
  pulse++;
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}
