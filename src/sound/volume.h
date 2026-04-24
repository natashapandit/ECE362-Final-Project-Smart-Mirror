#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>

void volume_init(void);
void volume_update(void);
uint16_t volume_get(void);

#endif