#include <Arduino.h>
#include <credentials.h>
#include <thingDescriptionTemplate.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <analogWrite.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <WiFi.h>
#include <driver/adc.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

HTTPClient http; //for gateway registration
String ipAddr;
String thingTitle;

String thingDescription;
String status = "available";

boolean ledIsOn = true;
int ledStatus = 1;

AsyncWebServer server(80);
AsyncEventSource events("/events/critical-plant-status");
 
const char* ssid = WIFI_SSID;
const char* password =  WIFI_PASSWORD;


#define LED_PIN 32 //GPIO Port of Blue Wifi-Status LED

#define RGB_PIN_RED 16
#define RGB_PIN_GREEN 17
#define RGB_PIN_BLUE 18

int RGBValues [3] = {0, 255, 0}; //initialize RGB LED Green

#define DHT_PIN 4     // Digital pin connected to the DHT sensor
#define DHT_TYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht(DHT_PIN, DHT_TYPE);

#define BH1750_SCL 21
#define BH1750_SDA 22
BH1750 lightMeter;

#define ADCAttenuation ADC_ATTEN_DB_11  //ADC_ATTEN_DB_11 = 0-3,6V? Docs: 2,6V gut Messbar; Dämpung ADC
#define NUM_MOISTURE_READS 10
#define MOISTURE_SENSOR_MIN 1050 //Luft 1050; TODO: trockene Erde Messen
#define MOISTURE_SENSOR_MAX 2540 //max mit Wasser: 2540; frisch gegossen nach paar min 2350

#define HUMIDITY            0
#define TEMPERATURE         1
#define LIGHT_INTENSITY     2
#define MOISTURE            3

float sensorValues [4];

float humidityMin, humidityMax, tempMin, tempMax, lightMin, lightMax, moistureMin, moistureMax;

boolean eventSent= false;

Preferences preferences;

String getStringFromPreferences(const char * key, String defaultValue){
  preferences.begin("storage", false);
  String value = preferences.getString(key, defaultValue);
  preferences.end();
  return value;
}

int getIntFromPreferences(const char * key, int defaultValue){
  preferences.begin("storage", false);
  int value = preferences.getInt(key, defaultValue);
  preferences.end();
  return value;
}

float getFloatFromPreferences(const char * key, float defaultValue){
  preferences.begin("storage", false);
  float value = preferences.getFloat(key, defaultValue);
  preferences.end();
  return value;
}

void putStringIntoPreferences(const char * key, String value){
  preferences.begin("storage", false);
  preferences.putString(key, value);
  preferences.end();
}

void putIntIntoPreferences(const char * key, int value){
  preferences.begin("storage", false);
  preferences.putInt(key, value);
  preferences.end();
}

void putFloatIntoPreferences(const char * key, float value){
  preferences.begin("storage", false);
  preferences.putFloat(key, value);
  preferences.end();
}

void writeRGBValuesToPins(int rgb [3]){
  analogWrite (RGB_PIN_RED, rgb[0]);
  analogWrite (RGB_PIN_GREEN, rgb[1]);
  analogWrite (RGB_PIN_BLUE, rgb[2]);
}

void toggleLedStatus(){
  ledIsOn = !ledIsOn;
  if (ledIsOn){
    digitalWrite(LED_PIN, HIGH);	// turn on the LED
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }
}

void notFound(AsyncWebServerRequest *request){
  if (request->method() == HTTP_OPTIONS) {
    request->send(200);
  } else {
    request->send(404, "application/json", "{\"message\":\"Not found\"}");;
  }
}

String createSensorValuesString(){
  String sensorValuesString = "";
  sensorValuesString += ("Humidity: ");
  sensorValuesString += sensorValues[HUMIDITY];
  sensorValuesString += ("%, Temperature: ");
  sensorValuesString += sensorValues[TEMPERATURE];
  sensorValuesString += " Degree Celsius";
  sensorValuesString += (" Light Intensity: ");
  sensorValuesString += sensorValues[LIGHT_INTENSITY];
  sensorValuesString += (" --- Moisture Value: ");
  sensorValuesString += sensorValues[MOISTURE];
  return sensorValuesString;
}

void checkWifiAndUpdateStatusLED(){
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }
}

float getMoisturePercentage(float value){
  value = value - MOISTURE_SENSOR_MIN;
  if (value <= 0){
    return 0;
  }

  int range = MOISTURE_SENSOR_MAX - MOISTURE_SENSOR_MIN;

  if (range <= 0){
    return 0;
  }

  value = (value / range) * 100;

  if (value >= 100){
    return 100;
  } 

  return  value;
}

boolean humidityIsCritical(){
  return sensorValues[HUMIDITY] > humidityMax || sensorValues[HUMIDITY] < humidityMin;
}

boolean tempIsCritical(){
  return sensorValues[TEMPERATURE] > tempMax || sensorValues[TEMPERATURE] < tempMin;
}

boolean lightIsCritical(){
  return sensorValues[LIGHT_INTENSITY] > lightMax || sensorValues[LIGHT_INTENSITY] < lightMin;
}

boolean moistureIsCritical(){
  return sensorValues[MOISTURE] > moistureMax || sensorValues[MOISTURE] < moistureMin;
}

boolean anyValueIsCritical(){
  return humidityIsCritical() || tempIsCritical() || lightIsCritical() || moistureIsCritical();
}


void updateIPGateway(){
  // Path to Gateway Thing registration
  http.begin("https://plantfriend.ddns.net/gateway/add-thing");

  String bearerToken = "Bearer ";
  bearerToken += GATEWAY_API_TOKEN;

  http.addHeader("Authorization", bearerToken);

  // If you need an HTTP request with a content type: application/json, use the following:
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST("{\"title\":\"" + thingTitle + "\",\"localIP\":\"http://"+ ipAddr +"\"}");

  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);

  // Free resources
  http.end();
}


// Setup
void setup() {
  Serial.begin(9600);
  Serial.println("Booting");

  status = getStringFromPreferences("status", "available");
  ledStatus = getIntFromPreferences("ledStatus", 3);

  humidityMin = getFloatFromPreferences("humidityMin", 0.0);
  humidityMax = getFloatFromPreferences("humidityMax", 100.0);

  tempMin = getFloatFromPreferences("tempMin", 15.0);
  tempMax = getFloatFromPreferences("tempMax", 35.0);

  lightMin = getFloatFromPreferences("lightMin", 10.0);
  lightMax = getFloatFromPreferences("lightMax", 1000.0);

  moistureMin = getFloatFromPreferences("moistureMin", 1050);
  moistureMax = getFloatFromPreferences("moistureMax", 2540);

  writeRGBValuesToPins(RGBValues);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  ipAddr = WiFi.localIP().toString();

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  
  thingDescription = String(thingDescriptionTemplate);
  thingDescription.replace("&IP_ADDR&", ipAddr);

  // Get Thing Title from TD: JSON parsing
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, thingDescription);
  JsonObject TD_json = doc.as<JsonObject>();

  // You can use a String to get an element of a JsonObject
  // No duplication is done.
  thingTitle = TD_json[String("title")].as<String>();
  
  // setup pin 5 as a digital output pin for blue LED
  pinMode (LED_PIN, OUTPUT);

  dht.begin();

  // Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin(BH1750_SCL, BH1750_SDA);

  if (lightMeter.begin()) {
    Serial.println(F("BH1750 initialised"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));

  // Moisture Sensor
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADCAttenuation);
  }

  digitalWrite(LED_PIN, HIGH);	// turn on the LED

  
  // HTTP Server Routes 

  // Properties

  // read properties / HTTP GET
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", thingDescription);
    
    request->send(response);
  });

  server.on("/properties/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", status);
    
    request->send(response);
  });

  server.on("/properties/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(sensorValues[HUMIDITY]));
    
    request->send(response);
  });
  server.on("/properties/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(sensorValues[TEMPERATURE]));
    
    request->send(response);
  });
  server.on("/properties/light-intensity", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(sensorValues[LIGHT_INTENSITY]));
    
    request->send(response);
  });
  server.on("/properties/moisture", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(sensorValues[MOISTURE]));
    
    request->send(response);
  });

  server.on("/properties/humidityMin", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(humidityMin));
    
    request->send(response);
  });
  server.on("/properties/humidityMax", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(humidityMax));
    
    request->send(response);
  });
  server.on("/properties/tempMin", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(tempMin));
    
    request->send(response);
  });
  server.on("/properties/tempMax", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(tempMax));
  
  request->send(response);
});
  server.on("/properties/lightMin", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(lightMin));
  
  request->send(response);
});
  server.on("/properties/lightMax", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(lightMax));
  
  request->send(response);
});
  server.on("/properties/moistureMin", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(moistureMin));
  
  request->send(response);
});
  server.on("/properties/moistureMax", HTTP_GET, [](AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", String(moistureMax));
  
  request->send(response);
});

  //write properties / HTTP PUT

  AsyncCallbackJsonWebHandler *statusHandler = new AsyncCallbackJsonWebHandler("/properties/status", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["status"]){
      status = data["status"].as<String>();
    } else {
      status = "default";
    }

    putStringIntoPreferences("status", status);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *humidityMinHandler = new AsyncCallbackJsonWebHandler("/properties/humidityMin", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["humidityMin"]){
      humidityMin = data["humidityMin"].as<float>();
    }

    putFloatIntoPreferences("humidityMin", humidityMin);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *humidityMaxHandler = new AsyncCallbackJsonWebHandler("/properties/humidityMax", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["humidityMax"]){
      humidityMax = data["humidityMax"].as<float>();
    }

    putFloatIntoPreferences("humidityMax", humidityMax);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *tempMinHandler = new AsyncCallbackJsonWebHandler("/properties/tempMin", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["tempMin"]){
      tempMin = data["tempMin"].as<float>();
    }

    putFloatIntoPreferences("tempMin", tempMin);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *tempMaxHandler = new AsyncCallbackJsonWebHandler("/properties/tempMax", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["tempMax"]){
      tempMax = data["tempMax"].as<float>();
    }

    putFloatIntoPreferences("tempMax", tempMax);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *lightMinHandler = new AsyncCallbackJsonWebHandler("/properties/lightMin", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["lightMin"]){
      lightMin = data["lightMin"].as<float>();
    }

    putFloatIntoPreferences("lightMin", lightMin);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *lightMaxHandler = new AsyncCallbackJsonWebHandler("/properties/lightMax", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["lightMax"]){
      lightMax = data["lightMax"].as<float>();
    }

    putFloatIntoPreferences("lightMax", lightMax);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *moistureMinHandler = new AsyncCallbackJsonWebHandler("/properties/moistureMin", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["moistureMin"]){
      moistureMin = data["moistureMin"].as<float>();
    }

    putFloatIntoPreferences("moistureMin", moistureMin);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  AsyncCallbackJsonWebHandler *moistureMaxHandler = new AsyncCallbackJsonWebHandler("/properties/moistureMax", [](AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      data = json.as<JsonObject>();
    } 
    
    if (data["moistureMax"]){
      moistureMax = data["moistureMax"].as<float>();
    }

    putFloatIntoPreferences("moistureMax", moistureMax);

    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  server.addHandler(statusHandler);
  server.addHandler(humidityMinHandler);
  server.addHandler(humidityMaxHandler);
  server.addHandler(tempMinHandler);
  server.addHandler(tempMaxHandler);
  server.addHandler(lightMinHandler);
  server.addHandler(lightMaxHandler);
  server.addHandler(moistureMinHandler);
  server.addHandler(moistureMaxHandler);

  // Actions: HTTP POST

  server.on("/actions/toggle-led", HTTP_POST, [](AsyncWebServerRequest *request) {
    
    toggleLedStatus();

    StaticJsonDocument<100> data;
    data["new led-status"] = ledIsOn;
    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  server.on("/actions/reset-event", HTTP_POST, [](AsyncWebServerRequest *request) {
    
    eventSent = false;

    String response_str = "Critical Event succesfully reset.";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  server.on("/actions/set-rgb-led", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (request->hasParam("red")){
      RGBValues[0] = request->getParam("red")->value().toInt();
    }
    if (request->hasParam("green")){
      RGBValues[1] = request->getParam("green")->value().toInt();
    }
    if (request->hasParam("blue")){
      RGBValues[2] = request->getParam("blue")->value().toInt();
    }
    
    writeRGBValuesToPins(RGBValues);

    StaticJsonDocument<100> data;
    data["red:"] = RGBValues[0];
    data["green:"] = RGBValues[1];
    data["blue:"] = RGBValues[2];
    String response_str;
    serializeJson(data, response_str);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", response_str);
    
    request->send(response);
  });

  // Handle Web Server Events (SSE)
  events.onConnect([](AsyncEventSourceClient *client){
    //reconnect
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }

    //new client
    else {
      client->send("You are now subscribed to the critical-plant-status.", NULL, millis());

      if (anyValueIsCritical()){
        client->send("Critical Plant Status! Please check the properties.", NULL, millis());
      }
    }
  });

  server.addHandler(&events);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "append, delete, entries, foreach, get, has, keys, set, values, Authorization, Cache-Control, last-event-id");

  server.onNotFound(notFound);

  server.begin();

  updateIPGateway();

}

void loop() {

  ArduinoOTA.handle();

  checkWifiAndUpdateStatusLED();

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  sensorValues[HUMIDITY] = dht.readHumidity();
  // Read temperature as Celsius (the default)
  sensorValues[TEMPERATURE] = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  // float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(sensorValues[HUMIDITY]) || isnan(sensorValues[TEMPERATURE])) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }

  // Compute heat index in Fahrenheit (the default)
  // float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  // float hic = dht.computeHeatIndex(t, h, false);

  sensorValues[LIGHT_INTENSITY] = lightMeter.readLightLevel();

  int moistureSum = 0;

  for (int i = 0; i < NUM_MOISTURE_READS; i++) { // Averaging algorithm
    moistureSum += adc1_get_raw(ADC1_CHANNEL_6); //Read analog
  }
  sensorValues[MOISTURE] = getMoisturePercentage(moistureSum / NUM_MOISTURE_READS);

  Serial.println(createSensorValuesString());

  if (anyValueIsCritical()){
    int dangerArray [3] = {255,0,0};
    writeRGBValuesToPins(dangerArray);
    if (!eventSent){
      events.send("Critical Plant Status! Danger!", NULL, millis());
      eventSent = true;
    }
  } else {
    writeRGBValuesToPins(RGBValues);
  }

  updateIPGateway();

  // Wait a few seconds between measurements.
  delay(5000);
}