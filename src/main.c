#include <stdio.h>
#include "pico/stdlib.h"
#include "alarm_state.h"
#include "led_strip/light_ctrl.h"
#include "alarm/alarm.h"
#include "sound/sound.h"
#include "sound/volume.h"
#include "hstx/hstx.h"

volatile bool alarm_active = false;
volatile bool alarm_flash_state = false;

int main() {
    stdio_init_all();

    led_init();
    sound_init();
    volume_init();
    clock_init(10, 0, 0);
    alarm_set(10, 0, 10);

    display_update();
    display_init();

    uint32_t last_display_update = 0;

    while (1) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        clock_update();
        led_update();
        sound_update();
        volume_update();

        if (now - last_display_update >= 1000) {
            display_update();
            last_display_update = now;
        }

        sleep_ms(10);
    }

    return 0;
}