#include <Arduino.h>

#include "hal.h"

// GPIO 9 is ADC1_CH8 on the ESP32-S3 (safe to use alongside Wi-Fi)
const int BATTERY_ADC_PIN = 9;

// Divider multiplier (2.0 for two equal resistors, e.g., 100k + 100k)
// Adjust slightly (e.g., 2.02) if you measure a small offset with a multimeter
const float DIVIDER_MULTIPLIER = 2.0f;

// Hysteresis buffer in millivolts to prevent icon flickering
const int HYSTERESIS_MV = 40;

// Battery voltage thresholds (in mV)
const int THRESHOLD_FULL  = 3900;
const int THRESHOLD_3     = 3800;
const int THRESHOLD_2     = 3720;
const int THRESHOLD_1     = 3620;
const int THRESHOLD_EMPTY = 3300;

// Reads averaged battery voltage in millivolts
uint32_t readBatteryVoltageMV() {
    uint32_t totalMilliVolts = 0;
    const int numSamples = 16;

    for (int i = 0; i < numSamples; i++) {
        totalMilliVolts += analogReadMilliVolts(BATTERY_ADC_PIN);
        delay(2); // Short pause between samples
    }

    float avgPinMV = totalMilliVolts / (float)numSamples;
    return (uint32_t)(avgPinMV * DIVIDER_MULTIPLIER);
}

// Returns the appropriate battery level with hysteresis
BatteryLevel getBatteryLevel() {
    static BatteryLevel currentLevel = BAT_LEVEL_FULL;
    static bool initialized = false;

    int vBat = readBatteryVoltageMV();
    Serial.printf("Battery voltage: %d mV\n", vBat);

    // On first boot, snap directly to the closest level without hysteresis
    if (!initialized) {
        if (vBat >= THRESHOLD_FULL)        currentLevel = BAT_LEVEL_FULL;
        else if (vBat >= THRESHOLD_3)      currentLevel = BAT_LEVEL_3;
        else if (vBat >= THRESHOLD_2)      currentLevel = BAT_LEVEL_2;
        else if (vBat >= THRESHOLD_1)      currentLevel = BAT_LEVEL_1;
        else                               currentLevel = BAT_LEVEL_EMPTY;
        initialized = true;
    } else {
        // Apply hysteresis buffer when stepping up or down
        switch (currentLevel) {
            case BAT_LEVEL_FULL:
                if (vBat < (THRESHOLD_FULL - HYSTERESIS_MV)) currentLevel = BAT_LEVEL_3;
                break;

            case BAT_LEVEL_3:
                if (vBat >= (THRESHOLD_FULL + HYSTERESIS_MV))     currentLevel = BAT_LEVEL_FULL;
                else if (vBat < (THRESHOLD_3 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_2;
                break;

            case BAT_LEVEL_2:
                if (vBat >= (THRESHOLD_3 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_3;
                else if (vBat < (THRESHOLD_2 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_1;
                break;

            case BAT_LEVEL_1:
                if (vBat >= (THRESHOLD_2 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_2;
                else if (vBat < (THRESHOLD_1 - HYSTERESIS_MV))    currentLevel = BAT_LEVEL_EMPTY;
                break;

            case BAT_LEVEL_EMPTY:
                if (vBat >= (THRESHOLD_1 + HYSTERESIS_MV))        currentLevel = BAT_LEVEL_1;
                break;
        }
    }

    return currentLevel;
}
