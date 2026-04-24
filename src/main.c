#include "pico/stdlib.h"
#include "alarm_state.h"
#include "led_strip/light_ctrl.h"
#include "alarm/alarm.h"
#include "sound/sound.h"
#include "sound/volume.h"

// definition lives here — everyone else just extern's it via alarm_state.h
volatile bool alarm_active = false;

int main() {
    stdio_init_all();

    led_init();
    clock_init(10, 0, 0); // set current time
    alarm_set(10, 0, 10); //alarm time
    sound_init();
    volume_init();

    while (1) {
        clock_update();
        led_update();
        sound_update();
        volume_update();
        sleep_ms(10);
    }

    return 0;
}