#include "hal.h"

// Returns the appropriate battery level with hysteresis
BatteryLevel getBatteryLevel() {
    static int callCount = 0;

    callCount++;
    if (callCount > 4) {
        callCount = 0; // Reset after reaching the maximum level
    }

    switch (callCount) {
      case 0:
        return BAT_LEVEL_EMPTY;
      case 1:
        return BAT_LEVEL_1;
      case 2:
        return BAT_LEVEL_2;
      case 3:
        return BAT_LEVEL_3;
      case 4:
        return BAT_LEVEL_FULL;
    }
}
