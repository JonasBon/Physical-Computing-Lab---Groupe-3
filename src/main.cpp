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

// --- Camera setup ---
#define CAMERA_RX_PIN 16 // green wire
#define CAMERA_TX_PIN 17 // blue wire

const uint8_t END_MARKER[] = { 0xFF, 0xD9, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF }; // Beispiel
const size_t END_MARKER_LEN = sizeof(END_MARKER);

const int MAX_IMAGE_SIZE = 100 * 64;
uint8_t imageBuffer[MAX_IMAGE_SIZE];
size_t imageIndex = 0;

#define BUTTON_PIN_TAKE_IMAGE 18

unsigned long startTimeTimeout = 0;
const unsigned long timeout = 30000; // 30 seconds timeout
const unsigned long imageTimeout = 2000; // 2 seconds timeout for image capture

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

bool imageBufferEndsWithEndMarker(bool debug) {
    if (imageIndex < END_MARKER_LEN) return false;
    for (size_t i = 0; i < END_MARKER_LEN; ++i) {
        if(debug) {
            Serial.print("Checking imageBuffer at index: ");
            Serial.print(imageIndex - END_MARKER_LEN + i);
            Serial.print(" (");
            Serial.print(imageBuffer[imageIndex - END_MARKER_LEN + i], HEX);
            Serial.print(") ");
            Serial.print(" against end marker byte: ");
            Serial.println(END_MARKER[i], HEX);
        }
        if (imageBuffer[imageIndex - END_MARKER_LEN + i] != END_MARKER[i]) {
            return false;
        }
    }
    Serial.println("--- Image buffer ends with end marker! ---");
    return true;
}

void takeImage() {
    Serial2.println("capture"); // Send command to camera ESP to take a picture
    // reset image buffer
    memset(imageBuffer, 0, MAX_IMAGE_SIZE);
    imageIndex = 0;
    unsigned long start = millis();

    unsigned long timeDifference = 0;
    while (imageIndex < MAX_IMAGE_SIZE && !imageBufferEndsWithEndMarker(false)) {
        timeDifference = millis() - start;
        if(timeDifference > imageTimeout) {
            Serial.println("Timeout while waiting for image data");
            break;
        }
        if (Serial2.available() > 0) {
            imageBuffer[imageIndex++] = Serial2.read();
        }
    }
}


void setup() {
    Serial.begin(115200);
    Serial2.begin(57600, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN);

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

    // - Button setup -
    pinMode(BUTTON_PIN_TAKE_IMAGE, INPUT_PULLUP);
}

void loop() {

    if (digitalRead(BUTTON_PIN_TAKE_IMAGE) == LOW) {
        takeImage();
        //print image data to serial for debugging
//        Serial.println("Image data captured:");
//        for (int i = 0; i < MAX_IMAGE_SIZE; i++) {
//            Serial.print(imageBuffer[i], HEX);
//            Serial.print(" ");
//        }
//        Serial.println();

        Serial.println("Check for end marker:");
        bool snens = imageBufferEndsWithEndMarker(true);
        Serial.println("End marker found: " + String(snens));


        delay(1000); // Debounce delay
    }

//    while(!audio.isRunning() || (millis() - startTimeTimeout > timeout)) {
//        if (millis() - startTimeTimeout > timeout) {
//            Serial.println("Debug print: Audio took too long to start playing. :c");
//        }
//
//        String response = getAPIResponse("Erzähle einen kurzen random Witz. Bitte ohne Tabs oder Zeilenumbrüche, trenne alle Wörter ausschließlich mit Leerzeichen.");
//        Serial.println("Response from API: " + response);
//        playTTS(response);
//        startTimeTimeout = millis();
//
//        // LED light sensor readings
//        sensor_value = analogRead(PIN_LIGHT_SENSOR);
//        Serial.print("Sensor Value: ");
//        Serial.println(sensor_value);
//    }
//    audio.loop();
}