#include <ArduinoJson.h>

//------------------Source VIN---------------------

//int pinRef = A0;
float vRef;
int bitADC = 1023;

//------------------lCD I2C------------------------

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//-----------------DS18B20 Suhu----------------------

#include <OneWire.h>
#include <DallasTemperature.h>

#define one_wire_bus 2
OneWire oneWire(one_wire_bus); // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature sensor 

float temperature;

//-----------------TDS Meter--------------------

#define TdsSensorPin A2
#define SCOUNT  30            // sum of sample point

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float tdsVolt = 0;
float tdsValue = 0;

//-------------------Turbidity---------------------

int sensorPin = A3;

float suhu;
float turbVolt;
float ntu;
float turbidity;
// float Vclear = 4.726295;
float Vclear = 3.86999;
int turbSamples = 50;
int turbfilterArray[50];

//-------------------PH4502C---------------------
int analogInPin = A1;

float avgValue;
float pHVolt;
float voltFix;
float phValue;
float tempPH;
int samples = 50;
int filterArray[50]; // array to store data samples from sensor

//--------------------------------------------------
//--------------------------------------------------

void setup() {
  Serial.begin(250000);
  analogReference(DEFAULT);
  pinMode(TdsSensorPin, INPUT);
  //ph4502.init();
  lcd.begin();
}

void loop() {
  
  source_read();
  Suhu_read();
  TDS_read();
  Turbidity_read();
  PH_read();

  sensor_write();

  lcd_show();
}

void source_read(){
  vRef = 5; //analogRead(pinRef)*5/bitADC;
}

void Suhu_read(){
  sensors.requestTemperatures(); // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  temperature = sensors.getTempCByIndex(0);
}

void TDS_read(){
  for (int i=0; i<SCOUNT; i++){
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if(analogBufferIndex == SCOUNT){ 
      analogBufferIndex = 0;
    }
  }

  for(copyIndex=0; copyIndex<SCOUNT; copyIndex++){
    analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
  
    // read the analog value more stable by the median filtering algorithm, and convert to voltage value
    tdsVolt = getMedianNum(analogBufferTemp,SCOUNT) * vRef / bitADC;

    //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0)); 
    float compensationCoefficient = 1.0+0.02*(temperature - 25.0);
    //temperature compensation
    float compensationVoltage=tdsVolt/compensationCoefficient;
    
    //convert voltage value to tds value
    tdsValue = (133.42*compensationVoltage*compensationVoltage*compensationVoltage - 255.86*compensationVoltage*compensationVoltage + 857.39*compensationVoltage)*0.5;
    //tdsValue = 0.5427*tdsValue;
    tdsValue = 0.5019975*tdsValue; //f(x) = 0.925x
    tdsValue = round(tdsValue);
    
    //tdsValue = round_to_dp(tdsValue, 1);
  }

  // Serial.print("TDS Volt: ");
  // Serial.println(tdsVolt,2);
  // Serial.print("TDS Value: ");
  // Serial.print(tdsValue,0);
  // Serial.println(" ppm");
}

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen){
  int bTab[iFilterLen];
  for (byte i = 0; i<iFilterLen; i++)
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
  if ((iFilterLen & 1) > 0){
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

void Turbidity_read(){
  float ADCTurb = 0;
  turbVolt = 0;

  for (int i = 0; i < turbSamples; i++) {
    turbfilterArray[i] = analogRead(sensorPin);
  }

  for (int i = 0; i < turbSamples-1; i++) {
    for (int j = i + 1; j < turbSamples; j++) {
      if (turbfilterArray[i] > turbfilterArray[j]) {
        int swap = turbfilterArray[i];
        turbfilterArray[i] = turbfilterArray[j];
        turbfilterArray[j] = swap;
      }
    }
  }

  int remove = 20;
  ADCTurb = 0;
  for(int i = remove*1.5; i < turbSamples-remove/2; i++) 
  { 
    ADCTurb += turbfilterArray[i];
  }

  ADCTurb = ADCTurb/(turbSamples-2*remove);
  //ADCTurb = round_to_dp(ADCTurb,2);
  
  turbVolt = ADCTurb*vRef/bitADC;
  //turbVolt = round_to_dp(turbVolt,2);

  turbidity = 100.00 - (turbVolt / Vclear) * 100.00; // as relative percentage; 0% = clear water;
  //turbidity = round_to_dp(turbidity,1);
  if (turbidity < 0.0)
    turbidity = 0.0;
  if (turbidity > 100.0)
    turbidity = 100.0;

  //ntu = -1120.4*turbVolt*turbVolt+5742.3*turbVolt-4352.9;

  // Serial.print("Turb Volt: ");
  // Serial.println(turbVolt);
  // Serial.print("Turbidity: ");
  // Serial.print(turbidity);
  // Serial.println(" %");
}

void PH_read(){
  for (int i = 0; i < samples; i++) {
    filterArray[i] = analogRead(analogInPin);
    //filterArray[i] = ph4502.read_ph_level();
  }
  //tempPH = ph4502.read_temp();
  
  for (int i = 0; i < samples-1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (filterArray[i] > filterArray[j]) {
        int swap = filterArray[i];
        filterArray[i] = filterArray[j];
        filterArray[j] = swap;
      }
    }
  }

  int remove = 20;
  avgValue = 0;
  for(int i = remove/2; i < samples-remove*1.5; i++) 
  { 
    avgValue += filterArray[i];
  }

  avgValue = avgValue/(samples-2*remove);

  pHVolt = avgValue*vRef/bitADC;
  //pHVolt = avgValue;
  //pHVolt = avgValue*vRef/bitADC;
  voltFix = pHVolt;
  //voltFix = 0.713*pHVolt;
  //phValue = -5.7 * voltFix + 21.34;
  //phValue = -6.8742 * voltFix + 34.70504; //y = 1.206x + 8.969;
  phValue = -6.702345 * voltFix + 35.094414; //y = 0.975x + 1.257
  phValue = -7.46716009560 * voltFix + 39.26459255259;

  //phValue = -9.194 * pHVolt + 35.545; //kalibrasi dari dua titik 3.43v di pH 4.01, 3.12v di pH 6.86
  //phValue = -9.322716 * pHVolt + 36.77763;
  //phValue = -7.402236504 * pHVolt + 29.53643822; // f(x) = 0.794x - 0.333

  // Serial.print("Volt pH = ");
  // Serial.println(pHVolt);
  // Serial.print("Volt Fix = ");
  // Serial.println(voltFix);
  // Serial.print("pH = ");
  // Serial.println(phValue);
  // Serial.println();
}

float round_to_dp( float in_value, int decimal_place )
{
  float multiplier = powf( 10.0f, decimal_place );
  in_value = roundf( in_value * multiplier ) / multiplier;
  return in_value;
}

void sensor_write(){

  // Buat objek JSON
  StaticJsonDocument<500> doc;
  doc["suhu"] = temperature; //vRef; 
  doc["tds"] = tdsValue;
  doc["turb"] = turbidity; //turbVolt;
  doc["ph"] = phValue; //pHVolt;

  // Serialisasi objek JSON ke string
  String jsonString;
  serializeJson(doc, jsonString);

  // Kirim JSON ke ESP32
  Serial.println(jsonString);
}

void lcd_show(){
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Suhu: ");
  lcd.print(temperature);
  lcd.print(" C");

  lcd.setCursor(0,1);
  lcd.print("TDS : ");
  lcd.print(tdsValue,0);
  lcd.print(" ppm");

  delay(2000);

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Turb: ");
  lcd.print(turbidity);
  lcd.print(" %");

  lcd.setCursor(0,1);
  lcd.print("PH  : ");
  lcd.print(phValue);
  
  delay(2000);

  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Kondisi Air:");
  lcd.setCursor(0,1);
  if (tdsValue<120 && turbidity<2.0)
    lcd.print("Bersih");
  else
    lcd.print("Tidak Bersih");
  
  delay(2000);
}