
#include "piuio_config.h"

#ifdef ENABLE_WS2812_SUPPORT
#include "piuio_ws2812.h"
#include "piuio_ws2812_helpers.h"

#include "ws2812.pio.h"
#include "pico/multicore.h"

mutex_t mutex;
semaphore_t sem;

static struct lampArray* lamp;

void ws2812_update() {
    // Write lamp.data to WS2812Bs
    put_pixel(lamp->l1_halo ? ws2812_color[0] : urgb_u32(0, 0, 0));
    put_pixel(lamp->l2_halo ? ws2812_color[1] : urgb_u32(0, 0, 0));
    for (int i = 0; i < 4; i++)
        put_pixel(lamp->bass_light ? ws2812_color[2] : urgb_u32(0, 0, 0));
    put_pixel(lamp->r2_halo ? ws2812_color[3] : urgb_u32(0, 0, 0));
    put_pixel(lamp->r1_halo ? ws2812_color[4] : urgb_u32(0, 0, 0));
}

void ws2812_core1() {
    while (true) {
        ws2812_lock_mtx();
        ws2812_update();
        ws2812_unlock_mtx();
        sleep_ms(5);
    }
}

void ws2812_lock_mtx() {
    uint32_t owner = 0;

    sem_release(&sem);
    if (!mutex_try_enter(&mutex, &owner)) {
        mutex_enter_blocking(&mutex);
    }
}

void ws2812_unlock_mtx() {
    mutex_exit(&mutex);
}

void ws2812_init(struct lampArray* l) {
    mutex_init(&mutex);
    sem_init(&sem, 0, 2);
    lamp = l;

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, WS2812_IS_RGBW);

    multicore_launch_core1(ws2812_core1);
}
#endif