#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>

#include "secrets.h"
#include "pinconfig.h"
#include "prompts.h"
#include "database.h"

// --- Light sensor setup ---
#define LIGHT_THRESHOLD 3500
int sensor_value = 0;

// --- Audio setup ---
Audio* audio;

// --- Camera setup ---
const uint8_t END_MARKER[] = { 0xFF, 0xD9, 0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF };
const size_t END_MARKER_LEN = sizeof(END_MARKER);
const int MAX_IMAGE_SIZE = 100 * 256;
uint8_t imageBuffer[MAX_IMAGE_SIZE];
size_t imageIndex = 0;

unsigned long startTimeTimeout = 0;
unsigned long lastTimeFridgeClosed = 0;
bool errorOccurred = false;
const unsigned int maxFridgeOpenTime = 10; // in seconds
const unsigned long audioTimeout = 30000; // 30 seconds timeout for audio playback
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

void printRawImageData() {
    Serial.println("Raw image data:");
    for (size_t i = 0; i < imageIndex; i++) {
        if (imageBuffer[i] < 16) {
            Serial.print("0");
        }
        Serial.print(String(imageBuffer[i], HEX) + " ");
    }
    Serial.println("");
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

void playTTSAudio(const String& input) {
    String result = urlEncode(input);
    result.replace(" ", "+");
    String finalUrl = String(SERVER_URL) + "/tts?text=" + result;

    audio = new Audio();
    audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio->setVolume(50);
    audio->connecttohost(finalUrl.c_str());
    startTimeTimeout = millis();
    while (1) {
        audio->loop();
        if (millis() - startTimeTimeout > audioTimeout) {
            debugPrintln("Audio playback timeout reached.");
            break;
        }
        if(!audio->isRunning()) {
            debugPrintln("Audio playback finished.");
            break;
        }
    }
    audio->stopSong();
    delete audio;
}

String getBarcodeApiResponse() {
    // If PSRAM is big enough, remove next line
    return "";

    String outputText = "";
    String apiUrl = String(SERVER_URL) + "/scan";
    String payload = R"({"image":"data:image/jpeg;base64,)";
    payload += String(base64::encode(imageBuffer, imageIndex - END_MARKER_LEN));
    payload += R"("})";

    debugPrint("Payload for barcode api: ");
    debugPrintln(payload);

    //printRawImageData();

    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Content-Length", String(payload.length()));
    //http.addHeader("Authorization", "Bearer " + String(CHATGPT_API_KEY));
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);

        //check if product.name is there
        if (jsonDoc.containsKey("product") && jsonDoc["product"].containsKey("name")) {
            outputText = jsonDoc["product"]["name"].as<String>();
        } else {
            // If no product name, return barcode
            debugPrintln("Barcode could not be recognized");
            if (jsonDoc.containsKey("error")) {
                debugPrintln("Error was found as well: " + jsonDoc["error"].as<String>());
            }
        }
    } else {
        Serial.printf("(http response of barcode scanner was not 200) Error %i \n", httpResponseCode);
        debugPrintln("Response: " + http.getString());
    }
    http.end();
    return outputText;
}


String getChatgptAPIResponse(String inputText, bool sendImage = false) {
    String outputText = "";
    String apiUrl = "https://api.openai.com/v1/chat/completions";
    String payload;

    DynamicJsonDocument doc(MAX_IMAGE_SIZE + 512);
    doc["model"] = "gpt-4o";
    JsonArray messages = doc.createNestedArray("messages");
    JsonObject userMessage = messages.createNestedObject();
    userMessage["role"] = "user";

    if (sendImage) {
        JsonArray contentArray = userMessage.createNestedArray("content");

        JsonObject textObj = contentArray.createNestedObject();
        textObj["type"] = "text";
        textObj["text"] = inputText;

        JsonObject imageObj = contentArray.createNestedObject();
        imageObj["type"] = "image_url";
        imageObj["image_url"]["url"] = "data:image/jpeg;base64," + base64::encode(imageBuffer, imageIndex - END_MARKER_LEN);
    } else {
        userMessage["content"] = inputText;
    }
    serializeJson(doc, payload);

    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(CHATGPT_API_KEY));
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode == 200) {
        String response = http.getString();
        DynamicJsonDocument jsonDoc(1024);
        deserializeJson(jsonDoc, response);
        outputText = jsonDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        debugPrintln("ChatGPT API request failed");
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

bool stringContains(const String& str, const String& substring) {
    return str.indexOf(substring) != -1;
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

    // - Button setup -
    pinMode(BUTTON_PIN_INSERT, INPUT_PULLUP);
    pinMode(BUTTON_PIN_REMOVE, INPUT_PULLUP);
    pinMode(BUTTON_PIN_READ_DATABASE, INPUT_PULLUP);

    // - LED setup -
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(YELLOW_LED_PIN, OUTPUT);

    // - Database setup -
    initDatabase();

    lastTimeFridgeClosed = millis();


    // radio test
//    audio = new Audio();
//    audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
//    audio->setVolume(10);
//    audio->connecttohost("0n-80s.radionetz.de:8000/0n-70s.mp3");
//    while (1) {
//        audio->loop();
//    }
}

void loop() {
    if(isLightOn()) {
        digitalWrite(RED_LED_PIN, HIGH);

        // Check if any button was pressed for inserting or removing items
        if(digitalRead(BUTTON_PIN_INSERT) == LOW || digitalRead(BUTTON_PIN_REMOVE) == LOW) {
            bool insertAction = digitalRead(BUTTON_PIN_INSERT) == LOW;
            bool removeAction = digitalRead(BUTTON_PIN_REMOVE) == LOW;

            //Check that only one button was pressed
            if(insertAction != removeAction) {
                digitalWrite(YELLOW_LED_PIN, HIGH);
                if(takeImage()) {
                    debugPrintln("Image captured successfully. Image size: " + String(imageIndex) + " bytes");
                    String productName = getBarcodeApiResponse();

                    if(productName.length() == 0) {
                        //debugPrintln("No product name found from barcode API, using ChatGPT for image recognition.");
                        String prompt1 = PROMPT_IMAGE_RECOGNITION;
                        String prompt2 = getDatabaseAsStringWithoutAmount().c_str();
                        String prompt = prompt1 + prompt2;
                        debugPrintln("Prompt for ChatGPT image recognition: " + prompt);
                        productName = getChatgptAPIResponse(prompt, true);
                        if(stringContains(productName, "unbekannt") || stringContains(productName, "Unbekannt") || stringContains(productName, "unknown") || stringContains(productName, "Unknown")) {
                            productName == "";
                            errorOccurred = true;
                        } else {
                            debugPrintln("Product name recognized by ChatGPT: " + productName);
                        }
                    }

                    if(productName.length() > 0) {
                        if(insertAction) {
                            addItemToDatabase(productName.c_str());
                            String prompt1 = PROMPT_SYSTEM;
                            String prompt2 = PROMPT_ADD_PRODUCT;
                            String prompt = prompt1 + prompt2 + productName;
                            String ttsResponse = getChatgptAPIResponse(prompt);
                            debugPrintln("TTS Response (add product): " + ttsResponse);
                            playTTSAudio(ttsResponse);
                        }
                        if(removeAction) {
                            removeItemFromDatabase(productName.c_str());
                            String prompt1 = PROMPT_SYSTEM;
                            String prompt2 = PROMPT_REMOVE_PRODUCT;
                            String prompt = prompt1 + prompt2 + productName;
                            String ttsResponse = getChatgptAPIResponse(prompt);
                            debugPrintln("TTS Response (remove product): " + ttsResponse);
                            playTTSAudio(ttsResponse);
                        }
                        //printDatabase();
                    } else {
                        errorOccurred = true;
                        playTTSAudio("Es konnte kein Produktname erkannt werden. Bitte versuche es erneut.");
                    }
                }
                digitalWrite(YELLOW_LED_PIN, LOW);
            }
        }

        // Check if Button is pressed to read the database
        else if(digitalRead(BUTTON_PIN_READ_DATABASE) == LOW) {
            digitalWrite(YELLOW_LED_PIN, HIGH);
            //printDatabase();
            String prompt1 = PROMPT_SYSTEM;
            String prompt2 = "Es folgt die ausgelesene Datenbank von Lebensmitteln, die sich im Kühlschrank befinden. Bitte gebe uns einmal eine schöne Response zurück, die wir dann als TTS abspielen können, um vorzulesen, was sich alles im Kühlschrank befindet. Ignoriere hierfür die vorherige Zeitbeschränkung der Antwort von 10 Sekunden. Darf schon länger dauern.: ";
            prompt2 += getDatabaseAsString().c_str();
            String prompt = prompt1 + prompt2;
            String ttsResponse = getChatgptAPIResponse(prompt);
            debugPrintln("TTS Response (read database): " + ttsResponse);
            playTTSAudio(ttsResponse);
            digitalWrite(YELLOW_LED_PIN, LOW);
        }

        // Check if fridge is open for too long (5 minutes)
        else if (millis() - lastTimeFridgeClosed > maxFridgeOpenTime * 1000) {
            debugPrintln("Fridge has been open for too long, playing warning sound.");
            String prompt1 = PROMPT_SYSTEM;
            String prompt2 = PROMPT_FRIDGE_OPEN_FOR_TOO_LONG;
            String prompt = prompt1 + prompt2;
            String response = getChatgptAPIResponse(prompt);
            debugPrintln("Fridge open warning response: " + response);
            playTTSAudio(response);
        }

        if(errorOccurred) {
            //blink three times with red LED
            for (int i = 0; i < 3; i++) {
                digitalWrite(RED_LED_PIN, LOW);
                delay(300);
                digitalWrite(RED_LED_PIN, HIGH);
                delay(300);
            }
            errorOccurred = false;
        }
    } else {
        digitalWrite(RED_LED_PIN, LOW);
        lastTimeFridgeClosed = millis();
        delay(500);
    }
}