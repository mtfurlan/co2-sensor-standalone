#include "rtc.h"

RTC_PCF8523 rtc;

bool setRTC(uint32_t newTime) {
    // if initialized and newTime is within 5 seconds of curren ttime, end
    if (rtc.initialized() && !rtc.lostPower() && abs((int32_t)rtc.now().unixtime() - (int32_t)newTime) > 5 ) {
        return false;
    }
    Serial.println("setting RTC");
    rtc.adjust(DateTime(newTime));
    return true;
}
int initRTC()
{
    if (! rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        return 1;
    }
    DateTime compiled = DateTime(F(__DATE__), F(__TIME__));
    Serial.printf("compiled at %04d-%02d-%02d %02d:%02d:%02d,%d,%f,%f\r\n",
            compiled.year(), compiled.month(), compiled.day(), compiled.hour(), compiled.minute(), compiled.second());
    DateTime adjusted = compiled + TimeSpan(0,4,0,0); // adjust for timezone
    setRTC(adjusted.unixtime());

    if (! rtc.initialized() || rtc.lostPower()) {
        Serial.println("RTC is NOT initialized, set time from compiled time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(adjusted);
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
        //
        // Note: allow 2 seconds after inserting battery or applying external power
        // without battery before calling adjust(). This gives the PCF8523's
        // crystal oscillator time to stabilize. If you call adjust() very quickly
        // after the RTC is powered, lostPower() may still return true.
    }

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
    return 0;
}
