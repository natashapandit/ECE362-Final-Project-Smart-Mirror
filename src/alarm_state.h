#ifndef ALARM_STATE_H
#define ALARM_STATE_H

#include <stdbool.h>

extern volatile bool alarm_active;\

// high when alarm is active, resets in 10s or on alarm_clear()

#endif