#ifndef LIGHT_CTRL_H
#define LIGHT_CTRL_H

#include <stdbool.h>

typedef enum {
    MODE_OFF = 0,
    MODE_COOL,
    MODE_NEUTRAL,
    MODE_WARM,
    MODE_COUNT
} LightMode;

void led_init(void);
void led_update(void);

#endif