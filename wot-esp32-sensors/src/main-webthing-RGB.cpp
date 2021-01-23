#include <Arduino.h>
#include <credentials.h>
#include <rgbThingDescriptionTemplate.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <analogWrite.h>
#include <Wire.h>
#include <WiFi.h>
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
 
const char* ssid = WIFI_SSID;
const char* password =  WIFI_PASSWORD;


#define LED_PIN 32 //GPIO Port of Blue Wifi-Status LED

#define RGB_PIN_RED 16
#define RGB_PIN_GREEN 17
#define RGB_PIN_BLUE 18

int RGBValues [3] = {0, 255, 0}; //initialize RGB LED Green


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

void checkWifiAndUpdateStatusLED(){
  if (WiFi.status() == WL_CONNECTED){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }
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

  server.addHandler(statusHandler);

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

  updateIPGateway();

  writeRGBValuesToPins(RGBValues);

  // Wait a few seconds between measurements.
  delay(5000);
}