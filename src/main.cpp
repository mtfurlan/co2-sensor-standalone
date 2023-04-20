#include <Arduino.h>
#include <Wire.h>
#include <FastLED.h>
#include <U8g2lib.h>
#include "sdc41.h"
#include "rtc.h"
#include "sd.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// https://cdn.sparkfun.com/assets/learn_tutorials/8/5/2/ESP32ThingPlusV20.pdf
// https://cdn.sparkfun.com/assets/5/9/7/4/1/SparkFun_Thing_Plus_ESP32-WROOM_C_schematic2.pdf

#define LED_PIN     2 //Pin 2 on Thing Plus C is connected to WS2812 LED
#define SD_CS 5
#define OLED_SDA 14
#define OLED_SCL 32
char logFilename[] = "/log";


const char* ssid = "node42";
const char* password =  "we do what we must, because we can";

const char* host = "192.168.1.235";



char buf[256];
File logFile;
WiFiUDP ntpUDP;
NTPClient* timeClient;

#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1
#define BRIGHTNESS  25
CRGB leds[NUM_LEDS];


U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, U8X8_PIN_NONE);



void resolve_mdns_host(const char * host_name)
{
    Serial.printf("Query A: %s.local\r\n", host_name);

    esp_ip4_addr_t addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(host_name, 2000,  &addr);
    if(err){
        if(err == ESP_ERR_NOT_FOUND){
            Serial.println("Host was not found!");
            return;
        }
        Serial.println("Query Failed");
        return;
    }

    printf(IPSTR, IP2STR(&addr));
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


    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

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




bool wifiConnected = false;
void loop() {
    if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.print("wifi connected: ");
        Serial.println(WiFi.localIP());
        Serial.println(WiFi.gatewayIP());
        timeClient = new NTPClient(ntpUDP, WiFi.gatewayIP());
        timeClient->begin();
        //resolve_mdns_host("co2");
    }
    if(wifiConnected) {
        // if we get a new timestamp from ntp
        if(timeClient->update()) {
            Serial.print("got NTP time: ");
            Serial.println(timeClient->getFormattedTime());
            setRTC(timeClient->getEpochTime());
        }
    }
    uint16_t co2;
    float temperature;
    float humidity;
    if(measureCO2(&co2, &temperature, &humidity)) {
        DateTime utc = rtc.now();
        DateTime now = utc - TimeSpan(0,4,0,0); // adjust for timezone
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
        if(wifiConnected) {

#define INFLUX_HOST "http://co2.local:8086"
#define INFLUX_ORG "c03eba1974105e33"
#define INFLUX_BUCKET "co2"
#define INFLUX_TOKEN "jIq50k_ijcDotF5yvA41C2MJqo91zDaoOAw7PFxCvA11x8kVBdoSf28RKaUOVgJnInqNEfdQSRpqPVi6NEVbsg=="
            HTTPClient http;
            http.begin(INFLUX_HOST "/api/v2/write?org=" INFLUX_ORG "&bucket=" INFLUX_BUCKET "&precision=s");
            http.addHeader("Authorization", "Token " INFLUX_TOKEN);
            //--header "Content-Type: text/plain; charset=utf-8"
            //--header "Accept: application/json"
            // https://docs.influxdata.com/influxdb/v2.7/get-started/write/#line-protocol-element-parsing
            sprintf(buf, "co2,room=i3party temp=%f,hum=%f,co2=%d %d", temperature, humidity, co2, utc.unixtime());
            Serial.println(buf);
            int rc = http.POST(buf);
            Serial.printf("HTTP POST response code %d\r\n", rc);
        }
    }
    delay(1000);
}
