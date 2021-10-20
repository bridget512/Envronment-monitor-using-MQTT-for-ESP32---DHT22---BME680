#include "arduino_secrets.h" // create a file called "arduino_secrets.h" in the same location as this file.
#include <WiFi.h>
#include <PubSubClient.h>

#include <DHT.h>
#define DHTPIN_1 19
#define DHTTYPE DHT22  // DHT11 = Usually Blue || DHT22 = Usually White
DHT dht_1(DHTPIN_1, DHTTYPE);


#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C

float hum_weighting = 0.25; // so hum effect is 25% of the total air quality score
float gas_weighting = 0.75; // so gas effect is 75% of the total air quality score

float hum_score, gas_score;
float gas_reference = 250000;
float hum_reference = 40;
int   getgasreference_count = 0;

const int mosfetPin = 15;

/****************** WiFi Credentials ******************/
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqtt_server = SECRET_MQTT_SERVER;
const char* mqtt_server_user = SECRET_MQTT_SERVER_USER;
const char* mqtt_server_password = SECRET_MQTT_SERVER_PASSWORD;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println();
  //Setup MOSFET pin and default to low
  pinMode(mosfetPin, OUTPUT);
  digitalWrite(mosfetPin, LOW);

  dht_1.begin();

  Wire.begin();
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  } else Serial.println("Found a sensor");

  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_2X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
//  bme.setGasHeater(320, 150); // 320°C for 150 ms
// Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
//  GetGasReference();

} // setup()

// Connect to WiFi using the given credentials in arduino_secrets.h
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
} // setup_wifi()


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }

  Serial.println();

  // ************************************************************ Subscribe Logic

  // ********************************************************** Fan State
  // Change pin state based on receive payload from MQTT
  if (String(topic) == "printer/temp/fanState") {
    Serial.print("Changing output to ");
    if (messageTemp == "1") {
      Serial.println("on");
      digitalWrite(mosfetPin, HIGH);
    }
    else if (messageTemp == "0") {
      Serial.println("off");
      digitalWrite(mosfetPin, LOW);
    }
  }
} //callback()


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect

    if (client.connect("ESP8266Client", SECRET_MQTT_SERVER_USER, SECRET_MQTT_SERVER_PASSWORD)) {
      Serial.println("connected");

      // ************************************************************  Subscribe Topics
      client.subscribe("printer/temp/fanState");
      client.subscribe("printer/lights");

    }

    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
} // reconnect()

void loop() {

  if(!client.connected()){ reconnect(); }
  client.loop();

  //  long now = millis();
  if (millis() - previousMillis > 5000) {
    previousMillis = millis();

    // ************************************************************  Publish Topics
    sensor_dht_1();
    sensor_BME680();

  } // millisCounter
} // loop()



void sensor_dht_1(){
  float readTemp = dht_1.readTemperature();  
  char tempString[8]; // Convert the value to a char array
  dtostrf(readTemp, 1, 2, tempString); // "dtostrf()" turns floats into strings
//  Serial.print("Sensor 1 - Temperature: ");
//  Serial.println(tempString);
  
  client.publish("printer/temp/sensor1", tempString);
}

void sensor_BME680(){
/*
 This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.
 
 Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
 1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
 2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
 3. You may not, except with my express written permission, distribute or commercially exploit the content.
 4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.
 The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
 software use is visible to an end-user.
 See more at http://www.dsbird.org.uk
*/  
//  Serial.print("Temperature = ");
//  Serial.print(bme.readTemperature());
//  Serial.println("°C");

//  Serial.print("   Pressure = ");

//  Serial.print(bme.readPressure() / 100.0F);
//  Serial.println(" hPa");

//  Serial.print("   Humidity = ");
//  Serial.print(bme.readHumidity());
//  Serial.println("%");

//  Serial.print("        Gas = ");
//  Serial.print(bme.readGas());
//  Serial.println("R\n");

  //Calculate humidity contribution to IAQ index
  float current_humidity = bme.readHumidity();
  
  // Humidity +/-5% around optimum 
  if (current_humidity >= 38 && current_humidity <= 42){ hum_score = 0.25*100; }
  
  else { //sub-optimal
    if( current_humidity < 38) {hum_score = 0.25/hum_reference*current_humidity*100; }
    else{ hum_score = ((-0.25/(100-hum_reference)*current_humidity)+0.416666)*100; }
  }
  
  //Calculate gas contribution to IAQ index
  float gas_lower_limit = 5000;   // Bad air quality limit
  float gas_upper_limit = 50000;  // Good air quality limit 
  if (gas_reference > gas_upper_limit) gas_reference = gas_upper_limit; 
  if (gas_reference < gas_lower_limit) gas_reference = gas_lower_limit;
  gas_score = (0.75/(gas_upper_limit-gas_lower_limit)*gas_reference -(gas_lower_limit*(0.75/(gas_upper_limit-gas_lower_limit))))*100;
  
  //Combine results for the final IAQ index value (0-100% where 100% is good quality air)
  float air_quality_score = hum_score + gas_score;
//
//  Serial.println("Air Quality = "+String(air_quality_score,1)+"% derived from 25% of Humidity reading and 75% of Gas reading - 100% is good quality air");
//  Serial.println("Humidity element was : "+String(hum_score/100)+" of 0.25");
//  Serial.println("     Gas element was : "+String(gas_score/100)+" of 0.75");
//  if (bme.readGas() < 120000) Serial.println("***** Poor air quality *****");
//  Serial.println();
  if ((getgasreference_count++)%10==0) GetGasReference(); 
  
  String outputString = CalculateIAQ(air_quality_score);
//  Serial.println(outputString);
//  Serial.println("------------------------------------------------");
//  delay(2000);

  
  client.publish("printer/iaq/quality", outputString.c_str());
}

void GetGasReference(){
  // Now run the sensor for a burn-in period, then use combination of relative humidity and gas resistance to estimate indoor air quality as a percentage.
//  Serial.println("Getting a new gas reference value");
  int readings = 10;
  for (int i = 1; i <= readings; i++){ // read gas for 10 x 0.150mS = 1.5secs
    gas_reference += bme.readGas();
  }
  gas_reference = gas_reference / readings;
}

String CalculateIAQ(float score){
  String IAQ_text = "Air quality is ";
  score = (100-score)*5;
  if      (score >= 301)                  IAQ_text += "Hazardous";
  else if (score >= 201 && score <= 300 ) IAQ_text += "Very Unhealthy";
  else if (score >= 176 && score <= 200 ) IAQ_text += "Unhealthy";
  else if (score >= 151 && score <= 175 ) IAQ_text += "Unhealthy for Sensitive Groups";
  else if (score >=  51 && score <= 150 ) IAQ_text += "Moderate";
  else if (score >=  00 && score <=  50 ) IAQ_text += "Good";
  return IAQ_text;
}
