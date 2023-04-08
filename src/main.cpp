#include <Arduino.h>

// https://cdn.sparkfun.com/assets/learn_tutorials/8/5/2/ESP32ThingPlusV20.pdf
// https://cdn.sparkfun.com/assets/5/9/7/4/1/SparkFun_Thing_Plus_ESP32-WROOM_C_schematic2.pdf

#define SD_CS 5
#define LED_PIN     2 //Pin 2 on Thing Plus C is connected to WS2812 LED
#define OLED_SDA 14
#define OLED_SCL 32

#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <FS.h>
#include <SD.h>
#include <RTClib.h>
#include <FastLED.h>
#include <U8g2lib.h>

#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1

#define BRIGHTNESS  25

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);
CRGB leds[NUM_LEDS];

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
    leds[0] = CRGB::Red;
    FastLED.show();
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
    leds[0] = CRGB::Green;
    FastLED.show();
    return 0;
}
void initSDC41(void)
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

    Wire.begin(SDA, SCL);

    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
    FastLED.setBrightness( BRIGHTNESS );
    leds[0] = CRGB::Red;
    FastLED.show();


    u8g2.begin();

    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
    u8g2.drawStr(0,10,"Hello World!");	// write something to the internal memory
    u8g2.sendBuffer();					// transfer internal memory to the display

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
        Serial.println("data not ready");
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
        int len = snprintf(buf, 256, "%04d-%02d-%02d %02d:%02d:%02d,%d,%f,%f\n",
                now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(),
                co2, temperature, humidity);
        //int len = snprintf(buf, 256, "CO2: %d\tTemp: %f\tHumidity: %f\n", co2, temperature, humidity);
        Serial.print(buf);
        Serial.print("\r"); // booo
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(0,10,buf);
        u8g2.sendBuffer();
        if(logFile) {
            logFile.write((uint8_t*)buf, len);
            logFile.flush();
            Serial.println("logged to file");
            if(leds[0].g) {
                leds[0] = CRGB::Blue;
            } else {
                leds[0] = CRGB::Green;
            }
            FastLED.show();
        }
    }
}

void loop() {
    measureCO2();
    delay(1000);
}
