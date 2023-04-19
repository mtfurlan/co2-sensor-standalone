#include <SensirionI2CScd4x.h>
#include "sdc41.h"

SensirionI2CScd4x scd4x;

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



int initSDC41(void)
{

    scd4x.begin(Wire);


    uint16_t error;
    char errorMessage[256];
    // stop potentially previously started measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return 1;
    }

    uint16_t serial0;
    uint16_t serial1;
    uint16_t serial2;
    error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (error) {
        Serial.print("Error trying to execute getSerialNumber(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return 1;
    } else {
        printSerialNumber(serial0, serial1, serial2);
    }

    //uint16_t calibration = 0;
    //error = scd4x.getAutomaticSelfCalibration(calibration);
    //Serial.printf("get auto cal returned %d, cal: %d\r\n", error, calibration);

    //uint16_t sensorStatus = 0;
    //error = scd4x.performSelfTest(sensorStatus);
    //if (error) {
    //    Serial.printf("self test returned %d, sensorStatus: %d\r\n", error, sensorStatus);
    //} else {
    //    Serial.println("Self test passed");
    //}

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    //error = scd4x.startLowPowerPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return 1;
    }
    return 0;
}

bool measureCO2(uint16_t* co2, float* temperature, float* humidity)
{
    uint16_t error;
    char buf[256];


    // Read Measurement
    bool isDataReady = false;
    error = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
        Serial.print("Error trying to execute getDataReadyFlag(): ");
        errorToString(error, buf, 256);
        Serial.println(buf);
        return false;
    }
    if (!isDataReady) {
        Serial.print(".");
        return false;
    }
    Serial.println("");
    error = scd4x.readMeasurement(*co2, *temperature, *humidity);
    if (error) {
        Serial.print("Error trying to execute readMeasurement(): ");
        errorToString(error, buf, 256);
        Serial.println(buf);
    } else if (*co2 == 0) {
        Serial.println("Invalid sample detected, skipping.");
        return false;
    }
    return true;
}
