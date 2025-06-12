#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include "secrets.h"
#include "pinconfig.h"

// --- Light sensor setup ---
#define LIGHT_THRESHOLD 3500
int sensor_value = 0;

// --- Audio setup ---
Audio audio;

// --- Camera setup ---
const uint8_t END_MARKER[] = { 0xFF, 0xD9, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF };
const size_t END_MARKER_LEN = sizeof(END_MARKER);
const int MAX_IMAGE_SIZE = 100 * 256;
uint8_t imageBuffer[MAX_IMAGE_SIZE];
size_t imageIndex = 0;

unsigned long startTimeTimeout = 0;
const unsigned long timeout = 30000; // 30 seconds timeout
const unsigned long imageTimeout = 5000; // 5 seconds timeout for image capture


const bool DEBUG_MODE = true;

void debugPrint(String message) {
    if(DEBUG_MODE) {
        Serial.print(message);
    }
}

void debugPrintln(String message) {
    if(DEBUG_MODE) {
        Serial.println(message);
    }
}

bool isLightOn() {
    sensor_value = analogRead(PIN_LIGHT_SENSOR);
    //debugPrintln("Current light sensor value: " + String(sensor_value));
    return sensor_value < LIGHT_THRESHOLD;
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


String getAPIResponse(String inputText, bool sendImage = false) {

    String outputText = "Fehler bei der Kommunikation mit der API";
    String apiUrl = "https://api.openai.com/v1/chat/completions";
    String payload = "";

    if (sendImage) {
        // Bild in Base64 konvertieren
        String base64Image = base64::encode(imageBuffer, imageIndex - END_MARKER_LEN);
        String imageDataUrl = "data:image/jpeg;base64," + base64Image;

        // JSON-Payload mit Bild + Text
        payload = "{"
                  "\"model\": \"gpt-4o\","
                  "\"messages\": ["
                  "{"
                  "\"role\": \"user\","
                  "\"content\": ["
                  "{ \"type\": \"text\", \"text\": \"" + inputText + "\" },"
                  "{ \"type\": \"image_url\", \"image_url\": { \"url\": \"" + imageDataUrl + "\" } }""]""}""]""}";
    } else {
        // Nur Text-Prompt
        payload = "{"
                  "\"model\": \"gpt-4o\","
                  "\"messages\": ["
                  "{"
                  "\"role\": \"user\","
                  "\"content\": \"" + inputText + "\"""}""]""}";
    }

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

bool imageBufferEndsWithEndMarker() {
    if (imageIndex < END_MARKER_LEN) return false;
    for (size_t i = 0; i < END_MARKER_LEN; ++i) {
        if (imageBuffer[imageIndex - END_MARKER_LEN + i] != END_MARKER[i]) {
            return false;
        }
    }
    return true;
}

bool takeImage() {
    Serial2.println("capture");
    memset(imageBuffer, 0, MAX_IMAGE_SIZE);
    imageIndex = 0;
    unsigned long start = millis();
    unsigned long timeDifference = 0;
    while (!imageBufferEndsWithEndMarker()) {
        timeDifference = millis() - start;
        if(timeDifference > imageTimeout) {
            Serial.println("Image Capture Error: Timeout reached");
            return false;
        }
        if(imageIndex >= MAX_IMAGE_SIZE) {
            Serial.println("Image Capture Error: Buffer overflow");
            return false;
        }

        if (Serial2.available() > 0) {
            imageBuffer[imageIndex++] = Serial2.read();
        }
    }
    return true;
}


void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, CAMERA_RX_PIN, CAMERA_TX_PIN);

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
    pinMode(BUTTON_PIN_INSERT, INPUT_PULLUP);
    pinMode(BUTTON_PIN_REMOVE, INPUT_PULLUP);

    // - LED setup -
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);
}

void loop() {
    if(isLightOn()) {
        digitalWrite(RED_LED_PIN, HIGH);

        // Check if any button was pressed
        if(digitalRead(BUTTON_PIN_INSERT) == LOW || digitalRead(BUTTON_PIN_REMOVE) == LOW) {
            bool insertAction = digitalRead(BUTTON_PIN_INSERT) == LOW;
            bool removeAction = digitalRead(BUTTON_PIN_REMOVE) == LOW;

            //Check that only one button was pressed
            if(insertAction != removeAction) {
                digitalWrite(YELLOW_LED_PIN, HIGH);
                if(takeImage()) {
                    bool hasBarcode = false;
                    String productName = "";

                    // TODO: Check image for Barcode
                    // Das hier geht prinzipiell, aber wird ja denke ich nicht die Lösung am Ende. Barcodes einlesen konnte ChatGPT bei mir nicht.
                    // productName = getAPIResponse("Gebe mir in einem Wort zurück, ob sich auf dem Bild ein Barcode befindet oder nicht.", true);
                    // debugPrintln("Is there a Barcode? Recognized by ChatGPT: " + productName);

                    if(hasBarcode) {
                        // TODO: Handle Barcode

                    } else {
                        productName = getAPIResponse("Gebe in nur 1-2 Wörtern zurück, um was für ein Lebensmittel es sich auf dem Bild handelt.", true);
                        debugPrintln("Product name recognized by ChatGPT: " + productName);
                    }

                    if(insertAction) {
                        // TODO: Add product to inventory
                    }
                    if(removeAction) {
                        // TODO: Remove product from inventory
                    }
                }
                digitalWrite(YELLOW_LED_PIN, LOW);
            }
        }
        audio.loop();
    } else {
        digitalWrite(RED_LED_PIN, LOW);
        delay(500);
    }

    if(false) {
        // --- Testing stuff ---

        if (digitalRead(BUTTON_PIN_INSERT) == LOW) {
            Serial.println("Button pressed, starting image capture...");
            bool snens = takeImage();
            Serial.println("Image captured successfully: " + String(snens));
            Serial.println("Image data captured, current index: " + String(imageIndex));

            //print last END_MARKER_LEN bytes of imageBuffer
            Serial.print("Last 8 bytes of imageBuffer: ");
            for (size_t i = 0; i < END_MARKER_LEN; ++i) {
                if (imageBuffer[imageIndex - END_MARKER_LEN + i] < 16) {
                    Serial.print("0");
                }
                Serial.print(imageBuffer[imageIndex - END_MARKER_LEN + i], HEX);
                Serial.print(" ");
            }

            if(false) {
                Serial.println("raw image data:");
                for (size_t i = 0; i < imageIndex; i++) {

                    // if imageBuffer[i] is smaller than 16, print it as a two-digit hex value
                    if (imageBuffer[i] < 16) {
                        Serial.print("0");
                    }

                    Serial.print(imageBuffer[i], HEX);
                    Serial.print(" ");
                }
            }

            Serial.println("--- Image data capturing finished! ---");
            String imageResponse = getAPIResponse("Gebe in nur 1-2 Wörtern zurück, um was für ein Lebensmittel es sich auf dem Bild handelt.", true);
            Serial.println("Response from API: " + imageResponse);

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
//    }
//    audio.loop();
    }

}