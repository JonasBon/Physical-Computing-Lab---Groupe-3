#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// --- Light sensor setup ---
#define PIN_LIGHT_SENSOR 32
#define LIGHT_THRESHOLD 750
int sensor_value = 0;


// --- Audio setup ---
#define I2S_DOUT 22
#define I2S_BCLK 26
#define I2S_LRC  25

Audio audio;
// ------

unsigned long startTimeTimeout = 0;
const unsigned long timeout = 30000; // 30 seconds timeout

bool isLightOn() {
    return sensor_value > LIGHT_THRESHOLD;
}

String urlEncode(const String& str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == ' ') {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            code0 = ((c >> 4) & 0xf) + '0';
            if (((c >> 4) & 0xf) > 9) code0 = ((c >> 4) & 0xf) - 10 + 'A';
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

void playTTS(const String& input) {
    String result = urlEncode(input);
    result.replace(" ", "+");
    String finalUrl = String(TTS_URL) + "text=" + result;
    audio.connecttohost(finalUrl.c_str());
}


String getAPIResponse(String inputText) {

    Serial.println("encoded input: " + urlEncode(inputText));

    String apiUrl = "https://api.openai.com/v1/chat/completions";
    String payload = "{\"model\": \"gpt-3.5-turbo\", \"messages\": [{\"role\": \"user\", \"content\": \"" + inputText + "\"}]}";
    String outputText = "Fehler bei der Kommunikation mit der API";

    HTTPClient http;

    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(CHATGPT_API_KEY));

    int httpResponseCode = http.POST(payload);
    if (httpResponseCode == 200) {
        String response = http.getString();

        // Parse JSON response
        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);
        outputText = jsonDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        Serial.printf("Error %i \n", httpResponseCode);
    }
    http.end();
    return outputText;
}



void setup() {
    Serial.begin(115200);

    // - WiFi setup -
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    String ssid = WIFI_SSID;
    String password = WIFI_PASSWORD;
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

    // - Light sensor setup -
    pinMode(PIN_LIGHT_SENSOR, OUTPUT);
    digitalWrite(PIN_LIGHT_SENSOR, LOW);

    // - Audio setup -
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(10);
}

void loop() {
    while(!audio.isRunning() || (millis() - startTimeTimeout > timeout)) {
        if (millis() - startTimeTimeout > timeout) {
            Serial.println("Debug print: Audio took too long to start playing. :c");
        }

        String response = getAPIResponse("Erzähle einen kurzen random Witz. Bitte ohne Tabs oder Zeilenumbrüche, trenne alle Wörter ausschließlich mit Leerzeichen.");
        Serial.println("Response from API: " + response);
        playTTS(response);
        startTimeTimeout = millis();

        // LED light sensor readings
        sensor_value = analogRead(PIN_LIGHT_SENSOR);
        Serial.print("Sensor Value: ");
        Serial.println(sensor_value);
    }
    audio.loop();
}