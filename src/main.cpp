#include <Arduino.h>

// https://cdn.sparkfun.com/assets/learn_tutorials/8/5/2/ESP32ThingPlusV20.pdf

// i2c SDA 21
// i2c SCL 22

#define SD_CS 5
#define SD_CD 17

#include <SensirionI2CScd4x.h>
#include <Wire.h>

#include "FS.h"
#include "SD.h"

SensirionI2CScd4x scd4x;
File logFile;
char logFilename[] = "/log";

void printUint16Hex(uint16_t value) {
    Serial.print(value < 4096 ? "0" : "");
    Serial.print(value < 256 ? "0" : "");
    Serial.print(value < 16 ? "0" : "");
    Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
    Serial.print("Serial: 0x");
    printUint16Hex(serial0);
    printUint16Hex(serial1);
    printUint16Hex(serial2);
    Serial.println();
}

void teardownSD(void)
{
    SD.end();
    if(logFile) {
        logFile.close();
    }
    Serial.println("SD removed");
}
int initSD(void)
{
    if(!SD.begin(SD_CS)) {
        Serial.println("Card Mount Failed");
        return 1;
    }
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return 1;
    }
    logFile = SD.open(logFilename, FILE_APPEND, true);
    if(!logFile) {
        Serial.printf("File %s failed to open\r\n", logFilename);
        return 1;
    }
    Serial.println("SD mounted");
    return 0;
}
void initSDC41(void)
{

    Wire.begin(SDA, SCL); // default pins
    scd4x.begin(Wire);


    uint16_t error;
    char errorMessage[256];
    // stop potentially previously started measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        printSerialNumber(serial0, serial1, serial2);
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    Serial.println("Waiting for first measurement... (5 sec)");
}


bool checkSD = true;
void IRAM_ATTR sdChangeISR ()
{
    checkSD = true;
}
void handleSDMounting()
{
    if(digitalRead(SD_CD)) {
        initSD();
    } else {
        teardownSD();
    }
}

void setup(void)
{
	Serial.begin(115200);

    pinMode(SD_CD, INPUT_PULLUP);
    attachInterrupt(SD_CD, sdChangeISR, CHANGE);

    initSDC41();

    Serial.println("started");
}



void measureCO2()
{
    uint16_t error;
    char buf[256];


    // Read Measurement
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool isDataReady = false;
    error = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
        Serial.print("Error trying to execute getDataReadyFlag(): ");
        errorToString(error, buf, 256);
        Serial.println(buf);
        return;
    }
    if (!isDataReady) {
        return;
    }
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        Serial.print("Error trying to execute readMeasurement(): ");
        errorToString(error, buf, 256);
        Serial.println(buf);
    } else if (co2 == 0) {
        Serial.println("Invalid sample detected, skipping.");
    } else {
        int len = snprintf(buf, 256, "CO2: %d\tTemp: %f\tHumidity: %f\n", co2, temperature, humidity);
        Serial.print(buf);
        Serial.print("\r"); // booo
        if(logFile) {
            Serial.println("logging to file");
            logFile.write((uint8_t*)buf, len);
            logFile.flush();
        }
    }
}

void loop() {
    if(checkSD) {
        checkSD = false;
        handleSDMounting();
    }

    measureCO2();
    delay(100);
}
