#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>


extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
}


const int analog_pin = A0;
const int motion_pin = D7;

static const char* ssid = "eduroam";
static const char* username     = "";
static const char* password = "";

const String host = "http://api.arjen.io/";

String device_type = "device";
String shared_secret = "";
String token = "";

int last_config_time;

StaticJsonDocument<200> sensor_values;
StaticJsonDocument<200> device_config;
String location;

JsonArray sensors = sensor_values.createNestedArray("sensors");
JsonObject motion_sensor = sensors.createNestedObject();

struct ReturnPayload {
  int http_code;
  String body;
};

void connect_to_wifi(const char* ssid, const char* password) {
  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  // WPA2 Connection starts here
  // Setting ESP into STATION mode only (no AP mode or dual mode)
  wifi_set_opmode(STATION_MODE);
  struct station_config wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  strcpy((char*)wifi_config.ssid, ssid);
  wifi_station_set_config(&wifi_config);
  wifi_station_clear_cert_key();
  wifi_station_clear_enterprise_ca_cert();
  wifi_station_set_wpa2_enterprise_auth(1);
  wifi_station_set_enterprise_identity((uint8*)username, strlen(username));
  wifi_station_set_enterprise_username((uint8*)username, strlen(username));
  wifi_station_set_enterprise_password((uint8*)password, strlen(password));
  wifi_station_connect();
  // WPA2 Connection ends here

  Serial.println();
  Serial.println("Waiting for connection and IP Address from DHCP");
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
   Generates an API access token

   @param host - API location
   @param uid - unique identifier (usually MAC-address)
   @param shared_secret - secret code to generate a private API token
   @param type - type of device to generate a token for ("device" or "app")
*/
const char* generate_token(String host, String uid, String shared_secret, String device_type) {
  String endpoint = host + "access/private";

  StaticJsonDocument<200> payload;
  payload["uid"] = uid;
  payload["shared_secret"] = shared_secret;
  payload["type"] = device_type;

  ReturnPayload rp = send_post_request(endpoint, token, payload);

  if (rp.http_code == 200) {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, rp.body);
    const char* token = doc["token"];
    return token;
  } else {
    Serial.println("HTTP Code: " + String(rp.http_code));
    Serial.println("HTTP Body: " + String(rp.body));
  }
}

StaticJsonDocument<200> get_config(String host) {
  String endpoint = host + "private/config";

  ReturnPayload rp = send_get_request(endpoint);
  if (rp.http_code == 200) {
    deserializeJson(device_config, rp.body);
    last_config_time = millis();

    Serial.println("Retrieving Config File: ");
    serializeJsonPretty(device_config, Serial);
    Serial.println("");

    return device_config;
  } else {
    Serial.println("HTTP Code: " + String(rp.http_code));
    Serial.println("HTTP Body: " + String(rp.body));
  }
}

boolean send_sensor_values(String host, StaticJsonDocument<200> sensor_values) {
  String endpoint = host + "private/sensor_values";

  ReturnPayload rp = send_post_request(endpoint, token, sensor_values);
  if (rp.http_code == 200) {
    return true;
  } else {
    Serial.println("HTTP Code: " + String(rp.http_code));
    Serial.println("HTTP Body: " + String(rp.body));
    return false;
  }
}

/**
   @brief Returns if the ESP8266 has a WiFi connection
*/
bool is_wifi_connected() {
  return WiFi.status() == WL_CONNECTED ? true : false;
}

/**
   @brief Returns MAC-address of the device
*/
String get_mac_address() {
  if (is_wifi_connected()) {
    return WiFi.macAddress();
  }
  Serial.println("WiFi not connected - No MAC-address found");
}

/**
   @brief Sends a POST request

   @param - URL to send POST request to
   @param - StaticJsonDocument containing the payload you want to send

   @return RetrunPayload - a custom data type containing the returned http code and payload body
*/
ReturnPayload send_post_request(String host, String token, StaticJsonDocument<200> doc) {
  HTTPClient http;
  http.begin(host); // Start HTTP connection to the host
  http.addHeader("Content-Type", "application/json"); //Is this required?
  http.addHeader("X-API-KEY", token);

  String output;
  serializeJson(doc, output); // Convert our JSON payload to a String so we can print and send it

  Serial.print("Host: ");
  Serial.println(host); // See what we're using as host

  Serial.print("Post payload: ");
  Serial.println(output); // See what we're using as payload

  int return_http_code = http.POST(output); // Send the POST request with JSON as a String and save the return HTTP code
  String return_payload = http.getString(); // Save the return payload of the POST request

  http.end(); // End the HTTP connection after payload has been sent and return payload has been handled

  ReturnPayload rp = {return_http_code, return_payload};
  return rp;
}

/**
   @brief Sends a GET request

   @param - URL to send GET request to

   @return RetrunPayload - a custom data type containing the returned http code and payload body
*/
ReturnPayload send_get_request(String host) {
  HTTPClient http;
  http.begin(host); // Start HTTP connection to the host

  http.addHeader("X-API-KEY", token);

  Serial.print("Host: ");
  Serial.println(host);

  int return_http_code = http.GET(); // Send the GET request and save the return HTTP code
  String return_payload = http.getString(); // Save the return payload of the GET request

  http.end(); // End the HTTP connection after return payload has been handled

  ReturnPayload rp = {return_http_code, return_payload};
  return rp;
}

bool detect_motion(const int pin) {
  int pir_value = digitalRead(pin);

  if (pir_value == 1) {
    return true;
  } else {
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(motion_pin, INPUT);

  connect_to_wifi(ssid, password); // ..Connect to WiFi

  if (is_wifi_connected) {
    /**
       Generate API access token
    */
    if (token == "default") {
      Serial.print("Generating new API token..-> ");
      token = generate_token(host, get_mac_address(), shared_secret, "device");
      Serial.println(token);
    }

    get_config(host); // Get config
  }
  Serial.println("Setup completed");
}

void loop() {
  if (is_wifi_connected) {

    /**
       Get CONFIG from API, does this every ~ minute.
    */
    if ((millis() - last_config_time) > 60000) { // 60000 is 1 minute
      get_config(host);
    }

    /**
       Check if device has a location assigned
    */
    JsonObject config_object = device_config["config"];
    JsonVariant location = config_object.getMember("location");

    while (location.as<String>() == "null") {
      Serial.println("Device must have location assigned");
      Serial.println("Retrying in 5 seconds");

      delay(5000);

      device_config = get_config(host);
    }

    /**
       Take measurements of sensors sensor values
    */
    if (detect_motion(motion_pin)) {
      motion_sensor["sensor_name"] = "motion";
      motion_sensor["value"] = "100";

      /**
        Send sensor values to API
      */
      if (send_sensor_values(host, sensor_values)) {
        Serial.println("Sensor values sent!");
      }
    }
  } else {
    Serial.println("WiFi is not connected. Trying to reconnect.");
    connect_to_wifi(ssid, password);
  }
  delay(1000); // execute once every 5 seconds, don't flood remote service
}
