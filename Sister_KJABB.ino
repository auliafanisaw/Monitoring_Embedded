#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>                                                    // esp8266 library
#include <WiFiUdp.h>
#include <FirebaseArduino.h>                                                // firebase library
#include <NTPClient.h>

#define ONE_WIRE_BUS D2                                                     // DS18B20 konek ke Pin D2

#define FIREBASE_HOST "monitoring-kjabb-default-rtdb.asia-southeast1.firebasedatabase.app"        // Project name address from firebase id
#define FIREBASE_AUTH "mV18zDwwvHbVDF1rXCR1Bqc6aakfqaK8NEMGId1S"            // Secret key generated from firebase
#define WIFI_SSID "bangtan"                                                 // Enter your wifi name
#define WIFI_PASSWORD "thththth"                                            // Enter your wifi password

int RL = 20;                                        //nilai RL =10 pada sensor
float m = -0.417;                                      //hasil perhitungan gradien
float b = 0.858;                                       //hasil perhitungan perpotongan
float Ro = 0.65;                                      //hasil pengukuran RO

#define MUXPinS0 D7
#define MUXPinS1 D6
#define MUXPinS2 D5
#define MUXPinS3 D0
#define ANALOG_INPUT A0

const int numReadings = 5;                            //nilai pengambilan sample pembacaan sebesar 5 kali
float readings[numReadings];      
int readIndex = 0;              
float total = 0;                  
float average = 0;

OneWire oneWire(ONE_WIRE_BUS);                                              //ds18b20
DallasTemperature sensor(&oneWire);                                         //ds18b20

const long utcOffsetInSeconds = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", utcOffsetInSeconds);
                                                
// Deklarasi raindrop
int pinDO = D1;   
//int pinAO = A0;
int DataDigital, DataAnalog;

void setup(){
  
  Serial.begin(9600);
  delay(500);     
  pinMode (pinDO, INPUT);
  pinMode (ANALOG_INPUT, INPUT);
  pinMode (MUXPinS0, OUTPUT);
  pinMode (MUXPinS1, OUTPUT);
  pinMode (MUXPinS2, OUTPUT); 
  pinMode (MUXPinS3, OUTPUT);

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);                              // connect to firebase
  sensor.begin();
  timeClient.begin();
}

void changeMux(int d, int c, int b, int a) {
  digitalWrite(MUXPinS0, a);
  digitalWrite(MUXPinS1, b);
  digitalWrite(MUXPinS2, c);
  digitalWrite(MUXPinS3, d); 
}

void loop(){
    
    timeClient.update();
    sensor.requestTemperatures();
    float c = sensor.getTempCByIndex(0);
    //DataDigital = digitalRead(pinDO);
    DataAnalog = analogRead(ANALOG_INPUT);
    int fireNoHujan = 0;
    int fireNoise = 1;
    int fireGerimis = 2;
    int fireSedang = 3;
    int fireDeras = 4;

    /********** Multiplexer ***************/
    float valueRain;
    float valueGas;
    changeMux(LOW, HIGH, HIGH, HIGH);
    valueRain = analogRead(ANALOG_INPUT); //sensor Raindrop
    changeMux(HIGH, LOW, LOW, LOW);
    valueGas = analogRead(ANALOG_INPUT); //sensor MQ135
    
    /*********** DS18B20 sensor ************/
    if (isnan(c)) {                                                                        // Check if any reads failed.
      Serial.println(F("Failed to read from DS18B20 sensor!"));
      return;
    }

    int timestamp = timeClient.getEpochTime();
    String s_timestamp = String(timestamp);
  
    Serial.print("Celcius: ");  Serial.print(c); Serial.println("°C");
    //String fireCelcius = String(c) + String("°C");                                         //convert integer to string
  
    Firebase.setInt("/sensors/water_temperature/records/" + s_timestamp + "/created_at", timestamp);
    Firebase.setFloat("/sensors/water_temperature/records/" + s_timestamp + "/value", c);                                  

    /*********** MQ-135 sensor ************/
    float VRL; 
    float RS;  
    float ratio; 
     
    VRL = valueGas*(5/1023.0); //konversi analog ke tegangan
    RS =(5.0 /VRL-1)*RL ;//rumus untuk RS
    ratio = RS/Ro;  // rumus mencari ratio
    float ppm = pow(10, ((log10(ratio)-b)/m));//rumus mencari ppm
    
    total = total - readings[readIndex];
    readings[readIndex] = ppm;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
  
    if (readIndex >= numReadings) {
      readIndex = 0;
    }
  
    average = total / numReadings;
  
   //menampilkan nilai rata-rata ppm setelah 5 kali pembacaan
    Serial.print("amonia : ");
    Serial.print(average);
    Serial.println(" PPM");

    Firebase.setInt("/sensors/ammonia/records/" + s_timestamp + "/created_at", timestamp);          
    Firebase.setFloat("/sensors/ammonia/records/" + s_timestamp + "/value", average);

    /*********** Raindrop sensor ************/
    if (valueRain <= 1024 && valueRain > 1000){
      Serial.print("Tidak Hujan || ");  
      Serial.print("Data Analog = "); Serial.println(valueRain);
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/created_at", timestamp);       
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/value", fireNoHujan);                           
    }else if(valueRain < 1000 && valueRain >= 900){
      Serial.print("Noise || ");
      Serial.print("Data Analog = "); Serial.println(valueRain);
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/created_at", timestamp);          
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/value", fireNoise);
    }else if(valueRain < 900 && valueRain >= 750){
      Serial.print("Gerimis || ");
      Serial.print("Data Analog = "); Serial.println(valueRain);
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/created_at", timestamp);          
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/value", fireGerimis);
    }else if(valueRain < 750 && valueRain >= 600){
      Serial.print("Sedang || ");
      Serial.print("Data Analog = "); Serial.println(valueRain);
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/created_at", timestamp);          
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/value", fireSedang);
    }else{
      Serial.print("Deras || ");
      Serial.print("Data Analog = "); Serial.println(valueRain);
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/created_at", timestamp);          
      Firebase.setInt("/sensors/raindrops/records/" + s_timestamp + "/value", fireDeras);
    }

    //Serial.println(valueRain);
    Serial.println(timeClient.getEpochTime());

    Serial.println("");
    delay(5000);
    
    if (Firebase.failed()) {
      Serial.print("pushing /logs failed:");
      Serial.println(Firebase.error()); 
      return;
    }
          
    //Serial.print(timeClient.getHours());
    //Serial.print(":");
    //Serial.print(timeClient.getMinutes());
    //Serial.print(":");
    //Serial.println(timeClient.getSeconds());
    //Serial.println(timeClient.getEpochTime());
    //Serial.println(timeClient.getFormattedTime());
}
  
