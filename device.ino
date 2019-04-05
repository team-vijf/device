#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#ifndef STASSID
#define STASSID "TELE2-4A8EE8_2.4G"
#define STAPSK  "redacted"
#endif

const int pot_pin = A0;
const int led_pin = D2;
bool state = false;
int pot_value = 0;

const char* ssid     = STASSID;
const char* password = STAPSK;

const char* post_host = "redacted";
const char* get_host = "redacted";

struct ReturnPayload {
  int http_code;
  String body;
};

void connect_to_wifi(const char* ssid, const char* password) {
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); //Set ESP in WiFi-client mode
  WiFi.begin(ssid, password); //Connect to WAP with SSID and password

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Returns if the ESP8266 has a WiFi connection
 */
bool is_wifi_connected() {
  return WiFi.status() == WL_CONNECTED ? true : false;
}

/**
 * @brief Sends a POST request
 * 
 * @param - URL to send POST request to
 * @param - StaticJsonDocument containing the payload you want to send
 * 
 * @return RetrunPayload - a custom data type containing the returned http code and payload body
 */
ReturnPayload send_post_request(String post_host, StaticJsonDocument<200> doc) {
  HTTPClient http;
  http.begin(post_host); // Start HTTP connection to the host
  http.addHeader("Content-Type", "text/JSON"); //Is this required?
  
  String output;             
  serializeJson(doc, output); // Convert our JSON payload to a String so we can print and send it
  
  Serial.print("Post payload: ");
  Serial.println(output); // See what we're using as payload
  
  int return_http_code = http.POST(output); // Send the POST request with JSON as a String and save the return HTTP code
  String return_payload = http.getString(); // Save the return payload of the POST request
  
  http.end(); // End the HTTP connection after payload has been sent and return payload has been handled

  ReturnPayload rp = {return_http_code, return_payload};
  return rp;
}

/**
 * @brief Sends a GET request
 * 
 * @param - URL to send GET request to
 * 
 * @return RetrunPayload - a custom data type containing the returned http code and payload body
 */
ReturnPayload send_get_request(String get_host) {
  HTTPClient http;
  http.begin(get_host); // Start HTTP connection to the host

  Serial.print("Host: ");
  Serial.println(get_host);
   
  int return_http_code = http.GET(); // Send the GET request and save the return HTTP code
  String return_payload = http.getString(); // Save the return payload of the GET request
  
  http.end(); // End the HTTP connection after return payload has been handled

  ReturnPayload rp = {return_http_code, return_payload};
  return rp;
}

void setup() {
  Serial.begin(115200);

  pinMode(led_pin, OUTPUT);
  pinMode(pot_pin, INPUT);
  
  connect_to_wifi(ssid, password);

  Serial.println("Setup completed");
}

void loop() {
  state = pot_value > 100 ? true : false;
  state ? digitalWrite(led_pin, HIGH) : digitalWrite(led_pin, LOW);

  if (state == true) {
    if (is_wifi_connected()) {

      //Create JSON document to send to API
      StaticJsonDocument<200> doc;
      doc["movement_sensor_value"] = 1;
      doc["sound_sensor_value"] = 51;
      doc["uid"] = 123178592;

      // Call post or get request and put the returned http code/payload body in a custom data type
//      ReturnPayload return_payload = send_post_request(post_host, doc);
      ReturnPayload return_payload = send_get_request(get_host);
      
      Serial.print("HTTP return code: ");
      Serial.println(return_payload.http_code);
  
      Serial.print("Response payload: ");
      Serial.println(return_payload.body);
 
      delay(5000); // execute once every 5 seconds, don't flood remote service
    } else {
      Serial.println("WiFi is not connected. Trying to reconnect.");
      connect_to_wifi(ssid, password);
    }
  } else {
    Serial.println("Doing nothing. ");
    delay(1000);
  }
}
