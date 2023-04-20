#ifndef RTC_H
#define RTC_H

#include <RTClib.h>
extern RTC_PCF8523 rtc;

int initRTC();
bool setRTC(uint32_t time);

#endif // RTC_H
