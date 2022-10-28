#ifndef PIUIO_PICO_PIUIO_WS2812_H
#define PIUIO_PICO_PIUIO_WS2812_H

#include "piuio_structs.h"

void ws2812_init(struct lampArray* l);
void ws2812_lock_mtx();
void ws2812_unlock_mtx();

#endif //PIUIO_PICO_PIUIO_WS2812_H
