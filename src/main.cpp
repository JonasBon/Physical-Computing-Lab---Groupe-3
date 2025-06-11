#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

// define LED light sensor
#define PIN_LIGHT_SENSOR 32
#define LIGHT_THRESHOLD 750
int sensor_value = 0;

// define I2S connections
#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC  25

// create audio object
Audio audio;

AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;

// Wifi credentials
//String ssid = "hotspot10";
//String password = "snensen420";
String ssid ="";
String password = "";

const char* apiKey = "";

bool isLightOn() {
    return sensor_value > LIGHT_THRESHOLD;
}

void getAPIResponse() {
    // Send request to OpenAI API
    String inputText = "hallo wie geht es dir";
    String apiUrl = "https://api.openai.com/v1/chat/completions";
    String payload = "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"" + inputText + "\"}]}";

    HTTPClient http;

    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(apiKey));

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        String response = http.getString();

        // Parse JSON response
        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);
        String outputText = jsonDoc["choices"][0]["message"]["content"].as<String>();
        Serial.println(outputText);
    } else {
        Serial.printf("Error %i \n", httpResponseCode);
    }
}

void getAPIResponseWithSpeech() {
    String inputText = "Hallo, wie geht es dir?";
    String chatUrl = "https://api.openai.com/v1/chat/completions";
    String speechUrl = "https://api.openai.com/v1/audio/speech";

    // Schritt 1: ChatGPT-Antwort abrufen
    String chatPayload = "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"" + inputText + "\"}]}";

    HTTPClient http;
    http.begin(chatUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(apiKey));

    int httpResponseCode = http.POST(chatPayload);
    if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument jsonDoc(2048);
        deserializeJson(jsonDoc, response);
        String outputText = jsonDoc["choices"][0]["message"]["content"].as<String>();
        Serial.println("Antwort von ChatGPT:");
        Serial.println(outputText);

        // Schritt 2: Text in Sprache umwandeln
        audio.connecttohost("http://141.147.61.164:42069/tts?text=ich+habe+einen+dicken+und+würde+ihn+gerne+knicken");

    } else {
        Serial.printf("ChatGPT-Fehler: %i\n", httpResponseCode);
    }

    http.end();
}



void setup() {
    Serial.begin(115200);

    // define LED light sensor pin
    pinMode(PIN_LIGHT_SENSOR, OUTPUT);
    digitalWrite(PIN_LIGHT_SENSOR, LOW);

    // setup Wifi in station mode
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    // WiFi connected, print IP to serial monitor
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    Serial.println("");


    // connect MAX98357 I2S amplifier module
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

    // set volume (0-100)
    audio.setVolume(10);

    //getAPIResponseWithSpeech();
    audio.connecttohost("141.147.61.164:42069/tts?text=hallo+das+ist+ein+test+du+knecht");

    //audio.connecttohost("0n-80s.radionetz.de:8000/0n-70s.mp3");
    //audio.connecttohost("http://stream.ffn.de/radiobollerwagen/mp3-192/;stream.nsv?ref=radioplayer");
}

void loop() {
    // LED light sensor readings
    sensor_value = analogRead(PIN_LIGHT_SENSOR);
    //Serial.print("Sensor Value: ");
    //Serial.println(sensor_value);

    //getAPIResponse();

    // run audio player
    audio.loop();
}