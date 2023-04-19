#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include <U8g2lib.h>
#include "sdc41.h"
#include "rtc.h"
#include "sd.h"

// https://cdn.sparkfun.com/assets/learn_tutorials/8/5/2/ESP32ThingPlusV20.pdf
// https://cdn.sparkfun.com/assets/5/9/7/4/1/SparkFun_Thing_Plus_ESP32-WROOM_C_schematic2.pdf

#define LED_PIN     2 //Pin 2 on Thing Plus C is connected to WS2812 LED
#define SD_CS 5
#define OLED_SDA 14
#define OLED_SCL 32
char logFilename[] = "/log";


char buf[256];
File logFile;

#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1
#define BRIGHTNESS  25
CRGB leds[NUM_LEDS];


U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);

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

    uint8_t rc = 0;
    rc |= initRTC();
    rc |= initSD(SD_CS, logFilename, &logFile);
    rc |= initSDC41();

    if(rc != 0) {
        Serial.println("failed to setup");
        while (1) delay(10);
    }

    leds[0] = CRGB::Green;
    FastLED.show();
    Serial.println("started");
}




void loop() {
    uint16_t co2;
    float temperature;
    float humidity;
    if(measureCO2(&co2, &temperature, &humidity)) {
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
            size_t written = logFile.write((uint8_t*)buf, len);
            logFile.flush();
            if(written != len) {
                Serial.printf("wrote wrong length to file, expected %d, wrote %d\r\n", len, written);
            } else {
                if(leds[0].g) {
                    leds[0] = CRGB::Blue;
                } else {
                    leds[0] = CRGB::Green;
                }
                FastLED.show();
            }
        }
    }
    delay(1000);
}
