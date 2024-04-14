#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>

//--------------------------------------------------

float suhuValue;
int tdsValue;
float turbValue;
float phValue;

//--------------------------------------------------

const char* ssid = "";      // Nama SSID (jaringan WiFi)
const char* password = "";  // Kata sandi WiFi

WiFiClientSecure client;

const unsigned long interval = 6000;  // Interval waktu pembacaan data (dalam milidetik)
unsigned long lastMillis = 0;          // Waktu terakhir pembacaan

//boolean changeWifi = false;
int pinChangeWifi = D5;

void setup() {
  Serial.begin(250000);
  pinMode(pinChangeWifi, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFiManager wm;
   
  bool res;
  res = wm.autoConnect("I-Sansus WiFiManager","password");

  if(!res){
    Serial.println("Failed to connect");
  }
  else{
    Serial.println("Connected");

    client.setInsecure();
  }

}

void loop() {
  int pinPB = digitalRead(pinChangeWifi);
  
  if  (pinPB == LOW){
    //changeWifi = true;
    delay (1000);
    
    if (pinPB == LOW){
      //if (changeWifi){
      WiFi.mode(WIFI_STA);
      WiFiManager wm;
      wm.resetSettings();  
      bool res;
      res = wm.autoConnect("I-Sansus WiFiManager","password");

      if(!res){
        Serial.println("Failed to connect");
      }
      else{
        Serial.println("Connected");

        client.setInsecure();
      }
      //}
    }
    else{
      runProgram();
    }
  }
  else{
    runProgram();
  }
}

void runProgram(){
  sensors_read();

  if (!(suhuValue==0 && tdsValue==0 && turbValue==0 && phValue==0)){
    // Kirim data ke server jika terhubung ke WiFi
    if (WiFi.status() == WL_CONNECTED) {
        sendDataToServer();
    }
  }
}

void sensors_read(){
  if (Serial.available() > 0) {
    // Baca data JSON dari Arduino
    String jsonStr = Serial.readStringUntil('\n');

    // Parse JSON
    DynamicJsonDocument doc(900);
    DeserializationError error = deserializeJson(doc, jsonStr);

    // Cek kesalahan parsing
    if (!error) {
          // Ambil nilai sensor dari JSON
      suhuValue = doc["suhu"];
      tdsValue = doc["tds"];
      turbValue = doc["turb"];
      phValue = doc["ph"];
    } else {
      Serial.print("Gagal parsing JSON: ");
      Serial.println(error.c_str());
      return;

    }
  }
}

void sendDataToServer() {
  HTTPClient http;
  http.begin(client, "https://be-isansus... url backend");
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<900> doc;

  doc["suhu"] = suhuValue;
  doc["ph"] = phValue;
  doc["turbidity"] = turbValue;
  doc["tds"] = tdsValue;

  String requestBody;
  serializeJson(doc, requestBody);

  unsigned long startTime = millis();  // Waktu sebelum pengiriman data

  int httpCode = http.POST(requestBody);

  unsigned long endTime = millis();    // Waktu setelah pengiriman data
  unsigned long duration = endTime - startTime;

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP Code: " + String(httpCode));
    Serial.println("Server Response: " + payload);
    Serial.println("Duration: " + String(duration) + " ms");
  } else {
    Serial.println("Error mengirim request");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

