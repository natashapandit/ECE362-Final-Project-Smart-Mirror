#include "volume.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define VOLUME_PIN    45
#define VOLUME_ADC_CH 5

static uint16_t current_volume = 2048;

void volume_init(void) {
    adc_init();
    adc_gpio_init(VOLUME_PIN);
    printf("VOLUME: init done, using GPIO %d ADC%d\n", VOLUME_PIN, VOLUME_ADC_CH);
}

void volume_update(void) {
    adc_select_input(VOLUME_ADC_CH);
    current_volume = adc_read();
    //printf("VOLUME: %d\n", current_volume);
}

uint16_t volume_get(void) {
    return current_volume;
}