#include "sound.h"
#include "volume.h"
#include "alarm_state.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>

#define SOUND_PIN 36
#define N 256
#define RATE 20000
#define M_PI 3.14159265358979323846

static int step0 = 0;
static int offset0 = 0;
static uint16_t wavetable[N];
static bool sound_running  = false;

static void init_wavetable(void) {
    for (int i = 0; i < N; i++)
        wavetable[i] = (16383 * sin(2 * M_PI * i / N)) + 16384;
}

static void set_freq(float f) {
    if (f == 0.0f) {
        step0   = 0;
        offset0 = 0;
    } else {
        step0 = (f * N / RATE) * (1 << 16);
    }
}

static void pwm_audio_handler(void) {
    uint slice          = pwm_gpio_to_slice_num(SOUND_PIN);
    uint current_period = pwm_hw->slice[slice].top + 1;
    pwm_hw->intr        = (1u << slice);

    offset0 += step0;
    if (offset0 >= (N << 16))
        offset0 -= (N << 16);

    uint samp = wavetable[offset0 >> 16];
    samp = samp * current_period / (1 << 16);
    samp = samp * volume_get() / 4095;
    pwm_set_gpio_level(SOUND_PIN, samp);
}

static void start_sound(void) {
    uint slice = pwm_gpio_to_slice_num(SOUND_PIN);
    set_freq(440.0f);
    pwm_set_enabled(slice, true);
    sound_running = true;
    printf("SOUND: playing\n");
}

static void stop_sound(void) {
    uint slice = pwm_gpio_to_slice_num(SOUND_PIN);
    pwm_set_enabled(slice, false);
    pwm_set_gpio_level(SOUND_PIN, 0);
    set_freq(0.0f);
    sound_running = false;
    printf("SOUND: stopped\n");
}

void sound_init(void) {
    gpio_set_function(SOUND_PIN, GPIO_FUNC_PWM);
    uint slice   = pwm_gpio_to_slice_num(SOUND_PIN);
    pwm_set_clkdiv(slice, 150.f);
    uint period  = 1000000 / RATE - 1;
    pwm_set_wrap(slice, period);
    pwm_set_gpio_level(SOUND_PIN, 0);

    init_wavetable();

    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, pwm_audio_handler);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);

    pwm_set_enabled(slice, false);
    printf("SOUND: init done\n");
}

void sound_update(void) {
    static bool last_alarm = false;

    if (alarm_active && !last_alarm) {
        start_sound();
    } else if (!alarm_active && last_alarm) {
        stop_sound();
    }

    last_alarm = alarm_active;
}