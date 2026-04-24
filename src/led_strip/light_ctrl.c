#include "light_ctrl.h"
#include "alarm_state.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"
#include <stdio.h>

#define IS_RGBW false
#define NUM_PIXELS 100
#define WS2812_PIN 28
#define BUTTON_PIN 20
#define ALARM_FLASH_MS 300

#if WS2812_PIN >= NUM_BANK0_GPIOS
#error Attempting to use a pin>=32 on a platform that does not support it
#endif

static PIO pio;
static uint sm;

static LightMode current_mode = MODE_OFF;
static bool last_button = true;
static bool flash_state = false;
static uint32_t last_flash_time = 0;

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 8) | ((uint32_t)g << 16) | (uint32_t)b;
}

static inline void set_all_leds(uint32_t color) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        pio_sm_put_blocking(pio, sm, color << 8u);
    }
}

static uint32_t mode_to_color(LightMode mode) {
    switch (mode) {
        case MODE_OFF: return urgb_u32(0, 0, 0);
        case MODE_COOL: return urgb_u32(180, 200, 255);
        case MODE_NEUTRAL: return urgb_u32(255, 240, 200);
        case MODE_WARM: return urgb_u32(255, 160, 80);
        default: return urgb_u32(0, 0, 0);
    }
}

void led_init(void) {
    // button
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    // PIO
    uint offset;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &ws2812_program, &pio, &sm, &offset, WS2812_PIN, 1, true);

    if (!success) {
        printf("LED: failed to init PIO\n");
        return;
    }
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    set_all_leds(mode_to_color(current_mode));
    printf("LED init done, mode: OFF\n");
}

void led_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    static bool last_alarm_state = false;  // track previous alarm state

    if (alarm_active) {
        if (now - last_flash_time >= ALARM_FLASH_MS) {
            flash_state = !flash_state;
            set_all_leds(flash_state ? urgb_u32(255, 0, 0) : urgb_u32(0, 0, 0));
            last_flash_time = now;
        }
    } else {
        // alarm just cleared — restore current mode
        if (last_alarm_state) {
            set_all_leds(mode_to_color(current_mode));
            printf("LED restored to mode: %d\n", current_mode);
        }

        bool button = gpio_get(BUTTON_PIN);
        if (last_button && !button) {
            current_mode = (current_mode + 1) % MODE_COUNT;
            set_all_leds(mode_to_color(current_mode));
            printf("LED mode: %d\n", current_mode);
            sleep_ms(50);
        }
        last_button = button;
    }

    last_alarm_state = alarm_active;
}