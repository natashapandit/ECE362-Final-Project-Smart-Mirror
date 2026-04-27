#include "pico/stdlib.h"
#include "alarm_state.h"
#include "led_strip/light_ctrl.h"
#include "alarm/alarm.h"
#include "sound/sound.h"
#include "sound/volume.h"
#include "hstx/hstx.h"

// definition lives here — everyone else just extern's it via alarm_state.h
volatile bool alarm_active = false;

int main() {
    stdio_init_all();

    display_init();
    led_init();
    sound_init();
    volume_init();
    clock_init(10, 0, 0); // set current time
    alarm_set(10, 0, 10); //alarm time


    while (1) {
        clock_update();
        led_update();
        sound_update();
        volume_update();
        sleep_ms(10);
    }

    return 0;
}