#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebSrv.h>

#include <ArduinoJson.h>

#include <base64.h>

#include "index.h"

const char* ssid = ""; // INSERT WIFI NAME HERE
const char* password = ""; // INSERT WIFI PASSWORD HERE

const char* client_id = ""; // INSERT CLIENT ID
const char* client_secret = ""; // INSERT CLIENT SECRET
const char* redirect_uri = "http:///callback"; // INSERT https://(ip_address_of_esp32)/callback

const int pin_bttn_playpause = 26;
const int pin_bttn_next = 27;
const int pin_bttn_previous = 14;
const int pin_bttn_like = 12;

const int pin_pot = 34;

bool accessTokenSet = false;
String accessToken;
String refreshToken;
bool isPlaying = true;
int currentVol = 0;

AsyncWebServer server(80);

void setup()
{
  
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.println("Connecting to wifi...");
  }

  Serial.println("Connected to wifi.");
  Serial.println(WiFi.localIP());

  pinMode(pin_bttn_playpause, INPUT_PULLUP);
  pinMode(pin_bttn_next, INPUT_PULLUP);
  pinMode(pin_bttn_previous, INPUT_PULLUP);
  pinMode(pin_bttn_like, INPUT_PULLUP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", mainPage);
  });
  server.on("/callback", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!accessTokenSet)
    {
        if (request->arg("code") == "")
        { // Parameter not found
            char page[500];
            sprintf(page, errorPage, client_id, redirect_uri);
            request->send(200, "text/html", String(page)); // Send web page
        }
        else
        { // Parameter found
            if (getAccessToken(request->arg("code")))
            {
                request->send(200, "text/html", "Spotify setup complete Auth refresh in :" + String(500));
            }
            else
            {
                char page[500];
                sprintf(page, errorPage, client_id, redirect_uri);
                request->send(200, "text/html", String(page)); // Send web page
            }
            Serial.println(accessToken);
        }
    }
    else
    {
        request->send(200, "text/html", "Spotify setup complete");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
  
}

void loop()
{
  static bool pressed_play = false;
  static bool pressed_prev = false;
  static bool pressed_next = false;
  static bool pressed_like = false;

  if (accessToken.length() != 0) {
      // 
    if (!pressed_play && !digitalRead(pin_bttn_playpause)) {
      togglePlay();
      pressed_play = true;
    }
    if (digitalRead(pin_bttn_playpause)) {
      pressed_play = false;
    }
    // LIKE SONG
    if (!pressed_like && !digitalRead(pin_bttn_like)) {
      String songId = getTrackId();
      toggleLiked(songId);
      pressed_like = true;
    }
    if (digitalRead(pin_bttn_like)) {
      pressed_like = false;
    }
    // PREV SONG
    if (!pressed_prev && !digitalRead(pin_bttn_previous)) {
      skipBack();
      pressed_prev = true;
    }
    if (digitalRead(pin_bttn_previous)) {
      pressed_prev = false;
    }
    // NEXT SONG
    if (!pressed_next && !digitalRead(pin_bttn_next)) {
      skipForward();
      pressed_next = true;
    }
    if (digitalRead(pin_bttn_next)) {
      pressed_next = false;
    }

    int volRequest = map(analogRead(pin_pot), 0, 4095, 0, 100);
    if (abs(volRequest - currentVol) > 2) {
      adjustVolume(volRequest);
      currentVol = volRequest;
    }
  }
}

bool getAccessToken(String auth_code) { 
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");
    
    String auth = "Basic " + base64::encode(String(client_id) + ":" + String(client_secret));
    http.addHeader("Authorization", auth);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String requestBody = "grant_type=authorization_code&code=" + auth_code + "&redirect_uri=" + String(redirect_uri);
    
    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode == HTTP_CODE_OK) {
        String response = http.getString();
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        accessToken = String((const char*)doc["access_token"]);
        refreshToken = String((const char *)doc["refresh_token"]);
        Serial.println("Access Token: " + accessToken);
        return true;
    } else {
        Serial.println("Error: " + http.getString());
        return false;
    }
    http.end();
}

bool skipForward()
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/player/next";
        http.begin("https://api.spotify.com/v1/me/player/next");
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Length", "0");
        int httpResponseCode = http.POST("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            // String response = http.getString();
            Serial.println("skipping forward");
            success = true;
        }
        else
        {
            Serial.print("Error skipping forward: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }

        // Disconnect from the Spotify API
        http.end();
        return success;
    }

bool skipBack()
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/player/previous";
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Length", "0");
        int httpResponseCode = http.POST("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            // String response = https.getString();
            Serial.println("skipping backward");
            success = true;
        }
        else
        {
            Serial.print("Error skipping backward: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }

        // Disconnect from the Spotify API
        http.end();
        return success;
    }

bool togglePlay()
    {
        HTTPClient http;  
        String url = "https://api.spotify.com/v1/me/player/" + String(isPlaying ? "pause" : "play");
        isPlaying = !isPlaying;
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Length", "0");
        int httpResponseCode = http.PUT("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            // String response = https.getString();
            Serial.println((isPlaying ? "Playing" : "Pausing"));
            success = true;
        }
        else
        {
            Serial.print("Error pausing or playing: ");
            Serial.println(httpResponseCode);
        }

        // Disconnect from the Spotify API

        http.end();
        return success;
    }

bool toggleLiked(String songId)
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/tracks/contains?ids=" + songId;
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Type", "application/json");
        int httpResponseCode = http.GET();
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            String response = http.getString();
            http.end();
            if (response == "[ true ]")
            {
                dislikeSong(songId);
            }
            else
            {
                likeSong(songId);
            }
            success = true;
        }
        else
        {
            Serial.print("Error toggling liked songs: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
            http.end();
        }

        return success;
    }

bool likeSong(String songId)
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Type", "application/json");
        char requestBody[] = "{\"ids\":[\"string\"]}";
        int httpResponseCode = http.PUT(requestBody);
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            // String response = http.getString();
            Serial.println("added track to liked songs");
            success = true;
        }
        else
        {
            Serial.print("Error adding to liked songs: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }

        // Disconnect from the Spotify API
        http.end();
        return success;
    }
    bool dislikeSong(String songId)
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/tracks?ids=" + songId;
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        // http.addHeader("Content-Type","application/json");
        // char requestBody[] = "{\"ids\":[\"string\"]}";
        int httpResponseCode = http.sendRequest("DELETE");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 200)
        {
            // String response = http.getString();
            Serial.println("removed liked songs");
            success = true;
        }
        else
        {
            Serial.print("Error removing from liked songs: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }

        // Disconnect from the Spotify API
        http.end();
        return success;
    }

    String getTrackId() {
      HTTPClient http;
      String url = "https://api.spotify.com/v1/me/player/currently-playing";
      http.begin(url);
      String auth = "Bearer " + String(accessToken);
      http.addHeader("Authorization", auth);
      http.addHeader("Content-Length", "0");
      String songId = "";
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200)
      {
        String payload = http.getString();

        // Allocate the JSON document
        // Use a buffer size large enough to hold the JSON data
        DynamicJsonDocument doc(2048);

        // Parse the JSON
        DeserializationError error = deserializeJson(doc, payload);

        // Check for errors in parsing
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return songId;
        }

        // Get the "id" from the "item" object
        const char* itemId = doc["item"]["id"];
        songId = String(itemId);
      }
      else
      {
          Serial.print("Error getting track info: ");
          Serial.println(httpResponseCode);
          // String response = https.getString();
          // Serial.println(response);
          http.end();
      }
      
      return songId;
    }

bool adjustVolume(int vol)
    {
        HTTPClient http;
        String url = "https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(vol);
        http.begin(url);
        String auth = "Bearer " + String(accessToken);
        http.addHeader("Authorization", auth);
        http.addHeader("Content-Length", "0");
        int httpResponseCode = http.PUT("");
        bool success = false;
        // Check if the request was successful
        if (httpResponseCode == 204)
        {
            // String response = http.getString();
            Serial.println(vol);
            success = true;
        }
        else if (httpResponseCode == 403)
        {
            success = false;
            Serial.print("Error setting volume: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }
        else
        {
            Serial.print("Error setting volume: ");
            Serial.println(httpResponseCode);
            String response = http.getString();
            Serial.println(response);
        }

        // Disconnect from the Spotify API
        http.end();
        return success;
    }

