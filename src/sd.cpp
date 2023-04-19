#include "sd.h"
#include <SD.h>

void teardownSD(File* logFile)
{
    SD.end();
    if(*logFile) {
        logFile->close();
    }
    Serial.println("SD removed");
}
int initSD(int cs, char* logFilename, File* logFile)
{
    if (!SD.begin(cs)) {
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
    *logFile = SD.open(logFilename, FILE_APPEND, true);
    if(!*logFile) {
        Serial.printf("File %s failed to open\r\n", logFilename);
        return 1;
    }
    Serial.println("SD mounted");
    return 0;
}
