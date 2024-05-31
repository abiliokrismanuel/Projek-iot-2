// Library untuk mengakses pin analog dan serial
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

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
const char *topicdhthum = "Kelompok2/dht/humidity";
const char *topicdhttemp = "Kelompok2/dht/temperature";
const char *topictds = "Kelompok2/TDSvalue";
const char *topicwaterflow = "Kelompok2/WaterFlow(FLowRate)";
const char *topicliquid = "Kelompok2/WaterFlow(TotalLiquid)";


// Deklarasi or smth like that
char msg_tds[20];
char msg_dhttemp[20];
char msg_dhthum[20];
char msg_waterflowrate[20];
char msg_waterflowliquid[20];

#define TdsSensorPin A0
#define VREF 5     // analog reference voltage(Volt) of the ADC
#define SCOUNT  30           // sum of sample point
int analogBuffer[SCOUNT];    // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0,copyIndex = 0;
float averageVoltage = 0,tdsValue = 0,temperature = 10;

#define DHTPIN 4 //nanti sesuaiin pin digitalnya
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);


int waterPin = 2; //nanti sesuaiin pin digitalnya
volatile long pulse = 0; 
unsigned long lastTime = 0;

float volume = 0; 
float flowRate = 0; 


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

  pinMode(waterPin, INPUT); 
  attachInterrupt(digitalPinToInterrupt(waterPin), increase, RISING);
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

void loop()
{
    //For TDS
   static unsigned long analogSampleTimepoint = millis();
   if(millis()-analogSampleTimepoint > 40U)     //every 40 milliseconds,read the analog value from the ADC
   {
     analogSampleTimepoint = millis();
     analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
     analogBufferIndex++;
     if(analogBufferIndex == SCOUNT) 
         analogBufferIndex = 0;
   }   
   static unsigned long printTimepoint = millis();
   if(millis()-printTimepoint > 800U)
   {
      printTimepoint = millis();
      for(copyIndex=0;copyIndex<SCOUNT;copyIndex++)
        analogBufferTemp[copyIndex]= analogBuffer[copyIndex];
      averageVoltage = getMedianNum(analogBufferTemp,SCOUNT) * (float)VREF / 1024.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      float compensationCoefficient=1.0+0.02*(temperature-25.0);    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationVolatge=averageVoltage/compensationCoefficient;  //temperature compensation
      tdsValue=(133.42*compensationVolatge*compensationVolatge*compensationVolatge - 255.86*compensationVolatge*compensationVolatge + 857.39*compensationVolatge)*0.5; //convert voltage value to tds value
      //Serial.print("voltage:");
      //Serial.print(averageVoltage,2);
      //Serial.print("V   ");
      Serial.print("TDS Value:");
      Serial.print(tdsValue,0);
      Serial.println("ppm");
      dtostrf(tdsValue,2,2,msg_tds);
      client.publish(topictds, msg_tds);
   }

   //For DHT
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.print("Temperature:");
    Serial.print(temp);
    Serial.println("*C");
    dtostrf(hum, 4, 2, msg_dhthum);
    dtostrf(temp, 4, 2, msg_dhttemp);
    client.publish(topicdhttemp, msg_dhttemp);
    client.publish(topicdhthum, msg_dhthum);


  //For Waterflow
  volume = 2.663 * pulse / 1000.0; 

  // Menghitung kecepatan aliran air dalam liter per menit (L/m)
  flowRate = volume / ((millis() - lastTime) / 60000.0); // Menghitung kecepatan aliran dalam satuan L/m

  // Jika waktu yang berlalu lebih dari 2 detik, reset hitungan pulsa dan waktu
  if (millis() - lastTime > 2000) {
    pulse = 0; // Reset hitungan pulsa
    lastTime = millis(); // Reset waktu terakhir
  }

  // Mencetak volume dan kecepatan aliran ke Serial Monitor
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


  delay(500);
}


// Fungsi interrupt untuk meningkatkan hitungan pulsa
ICACHE_RAM_ATTR void increase() {
  pulse++;
}

// function support for tds
int getMedianNum(int bArray[], int iFilterLen) 
{
      int bTab[iFilterLen];
      for (byte i = 0; i<iFilterLen; i++)
      bTab[i] = bArray[i];
      int i, j, bTemp;
      for (j = 0; j < iFilterLen - 1; j++) 
      {
      for (i = 0; i < iFilterLen - j - 1; i++) 
          {
        if (bTab[i] > bTab[i + 1]) 
            {
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