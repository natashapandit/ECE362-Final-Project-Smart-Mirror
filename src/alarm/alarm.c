#include "alarm.h"
#include "alarm_state.h"
#include "pico/stdlib.h"
#include <stdio.h>

// clock state
static int current_hour, current_min, current_sec;
static uint32_t last_tick = 0;

// alarm state
static int  alarm_hour  = -1;
static int  alarm_min   = -1;
static int alarm_sec = -1;
static bool alarm_fired = false;
static uint32_t alarm_start = 0;

void clock_get(int *hour, int *min, int *sec) {
    *hour = current_hour;
    *min  = current_min;
    *sec  = current_sec;
}

void clock_init(int hour, int min, int sec) {
    current_hour = hour;
    current_min  = min;
    current_sec  = sec;
    last_tick    = to_ms_since_boot(get_absolute_time());
    printf("Clock set to %02d:%02d:%02d\n", current_hour, current_min, current_sec);
}

void alarm_set(int hour, int min, int sec) {
    alarm_hour  = hour;
    alarm_min   = min;
    alarm_sec   = sec;
    alarm_fired = false;
    printf("Alarm set for %02d:%02d:%02d\n", alarm_hour, alarm_min, alarm_sec);
}

void alarm_clear(void) {
    alarm_hour  = -1;
    alarm_min   = -1;
    alarm_sec   = -1;
    alarm_fired  = false;
    alarm_active = false;
    printf("Alarm cleared\n");
}

void clock_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // tick clock every second
    if (now - last_tick >= 1000) {
        last_tick = now;
        current_sec++;
        if (current_sec >= 60) {
            current_sec = 0;
            current_min++;
            if (current_min >= 60) {
                current_min = 0;
                current_hour = (current_hour + 1) % 24;
            }
        }
        printf("Time: %02d:%02d:%02d\n", current_hour, current_min, current_sec);
    }

    // trigger alarm if set and not yet fired
    if (alarm_hour != -1 && !alarm_fired && !alarm_active
        && current_hour == alarm_hour && current_min == alarm_min && alarm_sec == current_sec) {
        alarm_active = true;
        alarm_fired  = true;
        alarm_start  = to_ms_since_boot(get_absolute_time());
        printf("ALARM TRIGGERED\n");
    }

    // auto dismiss after 10 seconds (replace with button/joystick later)
    if (alarm_active && (now - alarm_start >= 10000)) {
        alarm_active = false;
        alarm_start  = 0;
        printf("ALARM DISMISSED\n");
    }
}