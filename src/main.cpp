#include <Arduino.h>

// https://cdn.sparkfun.com/assets/learn_tutorials/8/5/2/ESP32ThingPlusV20.pdf
// https://cdn.sparkfun.com/assets/5/9/7/4/1/SparkFun_Thing_Plus_ESP32-WROOM_C_schematic2.pdf

#define SD_CS 5

#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <FS.h>
#include <SD.h>
#include <RTClib.h>

SensirionI2CScd4x scd4x;
RTC_PCF8523 rtc;


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
    if (!SD.begin(SD_CS)) {
        Serial.println("Card Mount Failed");
        return 1;
    }
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return 1;
    }
    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
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

    Wire.begin();
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

void initRTC()
{
    if (! rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        while (1) delay(10);
    }

    if (! rtc.initialized() || rtc.lostPower()) {
        Serial.println("RTC is NOT initialized, let's set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
        //
        // Note: allow 2 seconds after inserting battery or applying external power
        // without battery before calling adjust(). This gives the PCF8523's
        // crystal oscillator time to stabilize. If you call adjust() very quickly
        // after the RTC is powered, lostPower() may still return true.
    }

    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

    // When the RTC was stopped and stays connected to the battery, it has
    // to be restarted by clearing the STOP bit. Let's do this to ensure
    // the RTC is running.
    rtc.start();

    // The PCF8523 can be calibrated for:
    //        - Aging adjustment
    //        - Temperature compensation
    //        - Accuracy tuning
    // The offset mode to use, once every two hours or once every minute.
    // The offset Offset value from -64 to +63. See the Application Note for calculation of offset values.
    // https://www.nxp.com/docs/en/application-note/AN11247.pdf
    // The deviation in parts per million can be calculated over a period of observation. Both the drift (which can be negative)
    // and the observation period must be in seconds. For accuracy the variation should be observed over about 1 week.
    // Note: any previous calibration should cancelled prior to any new observation period.
    // Example - RTC gaining 43 seconds in 1 week
    float drift = 43; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
    float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
    float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
    float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
    // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
    int offset = round(deviation_ppm / drift_unit);
    // rtc.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
    // rtc.calibrate(PCF8523_TwoHours, 0); // Un-comment to cancel previous calibration

    Serial.print("Offset is "); Serial.println(offset); // Print to control offset
}

void setup()
{
    Serial.begin(115200);

    initRTC();
    initSD();
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

        DateTime now = rtc.now();
        int len = snprintf(buf, 256, "%04d-%02d-%02d %02d:%02d:%02d CO2: %d\tTemp: %f\tHumidity: %f\n",
                now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
                co2, temperature, humidity);
        Serial.print(buf);
        Serial.print("\r"); // booo
        if(logFile) {
            logFile.write((uint8_t*)buf, len);
            logFile.flush();
            Serial.println("logged to file");
        }
    }
}

void loop() {
    measureCO2();
    delay(100);
}
