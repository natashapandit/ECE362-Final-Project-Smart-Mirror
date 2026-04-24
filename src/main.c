#include "pico/stdlib.h"
#include "led_strip/light_ctrl.h"
#include "alarm_state.h"

// definition lives here — everyone else just extern's it via alarm_state.h
volatile bool alarm_active = false;

int main() {
    stdio_init_all();

    led_init();
    clock_init(10, 0, 0); // set current time
    alarm_set(10, 0, 10); //alarm time

    while (1) {
        clock_update();
        led_update();
        sleep_ms(10);
    }

    return 0;
}