#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>

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

// Wifi credentials
String ssid = "ssid";
String password = "pw";

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

    audio.connecttohost("0n-80s.radionetz.de:8000/0n-70s.mp3");
}

void loop() {
    // LED light sensor readings
    sensor_value = analogRead(PIN_LIGHT_SENSOR);
    Serial.print("Sensor Value: ");
    Serial.println(sensor_value);

    // run audio player
    audio.loop();
}

bool isLightOn() {
    return sensor_value > LIGHT_THRESHOLD;
}