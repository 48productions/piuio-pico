#ifndef PIUIO_PICO_PIUIO_WS2812_HELPERS_H
#define PIUIO_PICO_PIUIO_WS2812_HELPERS_H

#include <stdint.h>
#include "hardware/pio.h"

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

#define urgb_u32(r, g, b) ((uint32_t) (r) << 8) |   \
                          ((uint32_t) (g) << 16) |  \
                          (uint32_t) (b)


#endif //PIUIO_PICO_PIUIO_WS2812_HELPERS_H
