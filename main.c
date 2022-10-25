#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "bsp/board.h"
#include "hardware/gpio.h"
#include "device/usbd.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

// WS2812 definitions
#define IS_RGBW false
#define NUM_PIXELS 8
#define WS2812_PIN 22

mutex_t mutex;
semaphore_t sem;

const uint8_t STATE_CAB_PLAYER_1 = 1;
const uint8_t STATE_PLAYER_1 = 0;
const uint8_t STATE_PLAYER_2 = 2;

// Set pins for the switches/LEDs
// Order of DL, UL, C, UR, DR for each player, then test and service switches
const uint8_t pinSwitch[12] = {19, 21, 10, 6, 8, 17, 27, 2, 0, 4, 15, 14}; // map according to your button pins
const uint8_t pinLED[10] = {18, 20, 11, 7, 9, 16, 26, 3, 1, 5}; // map according to your LED pins
const uint8_t pos[] = { 3, 0, 2, 1, 4 }; // don't touch this

// PIUIO input and output data
uint8_t inputData[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct lamp {
    union{
        uint8_t data[8];
        struct {
                // p1 cmd byte 0
                uint8_t empty : 2;
                uint8_t p1_ul_light : 1;
                uint8_t p1_ur_light : 1;
                uint8_t p1_cn_light : 1;
                uint8_t p1_dl_light : 1;
                uint8_t p1_dr_light : 1;
                uint8_t empty1 : 1;

                // p1 cmd byte 1
                uint8_t empty2 : 2;
                uint8_t bass_light : 1;
                uint8_t empty3 : 5;

                // p2 cmd byte 2
                uint8_t empty4 : 2;
                uint8_t p2_ul_light : 1;
                uint8_t p2_ur_light : 1;
                uint8_t p2_cn_light : 1;
                uint8_t p2_dl_light : 1;
                uint8_t p2_dr_light : 1;
                uint8_t r2_halo : 1;

                // p2 cmd byte 3
                uint8_t r1_halo : 1;
                uint8_t l2_halo : 1;
                uint8_t l1_halo : 1;
                uint8_t empty5 : 3;
                uint8_t r1_halo_dupe : 1;
                uint8_t r2_halo_dupe : 1;

                uint32_t empty6 : 32;
        };
    };
};

struct lamp lamp = {};

uint32_t led_color[5][3] = {
  {0, 255, 0},      // Lower left
  {255, 0, 0},      // Upper left
  {0, 0, 255},        // Bass/Neon
  {255, 0, 0},      // Upper right
  {0, 255, 0},      // Lower right
};

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    // Request 0xAE = IO Time
    if (request->bRequest == 0xAE) {
        switch (request->bmRequestType) {
            case 0x40:
                return tud_control_xfer(rhport, request, (void *)&lamp.data, 8);
            case 0xC0:
                return tud_control_xfer(rhport, request, (void *)&inputData, 8);
            default:
                return false;
        }
    }

    return false;
}

void ws2812b_update() {
    // Write lamp.data to WS2812Bs
    put_pixel(lamp.l1_halo ? urgb_u32(led_color[0][0], led_color[0][1], led_color[0][2]) : urgb_u32(0, 0, 0));
    put_pixel(lamp.l2_halo ? urgb_u32(led_color[1][0], led_color[1][1], led_color[1][2]) : urgb_u32(0, 0, 0));
    for (int i; i < 4; i++) {
      put_pixel(lamp.bass_light ? urgb_u32(led_color[2][0], led_color[2][1], led_color[2][2]) : urgb_u32(0, 0, 0));
    }
    put_pixel(lamp.r2_halo ? urgb_u32(led_color[3][0], led_color[3][1], led_color[3][2]) : urgb_u32(0, 0, 0));
    put_pixel(lamp.r1_halo ? urgb_u32(led_color[4][0], led_color[4][1], led_color[4][2]) : urgb_u32(0, 0, 0));
}

void core1_entry() {
    while (1) {
        uint32_t owner = 0;

        sem_release(&sem);
        if (!mutex_try_enter(&mutex, &owner)) {
            mutex_enter_blocking(&mutex);
        }

        ws2812b_update();

        mutex_exit(&mutex);

        sleep_ms(5);
    }
}

void piuio_task(void) {
    uint32_t owner = 0;

    sem_release(&sem);
    if (!mutex_try_enter(&mutex, &owner)) {
        mutex_enter_blocking(&mutex);
    }

    // P1 / P2 inputs
    for (int i = 0; i < 5; i++) {
        uint8_t* p1 = &inputData[STATE_PLAYER_1];
        uint8_t* p2 = &inputData[STATE_PLAYER_2];
        *p1 = gpio_get(pinSwitch[i]) ? tu_bit_set(*p1, pos[i]) : tu_bit_clear(*p1, pos[i]);
        *p2 = gpio_get(pinSwitch[i+5]) ? tu_bit_set(*p2, pos[i]) : tu_bit_clear(*p2, pos[i]);
    }

    // Test/Service buttons
    inputData[STATE_CAB_PLAYER_1] = gpio_get(pinSwitch[10]) ? tu_bit_set(inputData[1], 1) : tu_bit_clear(inputData[1], 1);
    inputData[STATE_CAB_PLAYER_1] = gpio_get(pinSwitch[11]) ? tu_bit_set(inputData[1], 6) : tu_bit_clear(inputData[1], 6);


    // Write pad lamps
    for (int i = 0; i < 5; i++) {
        gpio_put(pinLED[i], tu_bit_test(lamp.data[STATE_PLAYER_1], pos[i] + 2));
        gpio_put(pinLED[i+5], tu_bit_test(lamp.data[STATE_PLAYER_2], pos[i] + 2));
    }

    // Write the bass neon to the onboard LED for testing + kicks
    gpio_put(25, lamp.bass_light);

    mutex_exit(&mutex);
}

int main(void) {
    board_init();

    mutex_init(&mutex);
    sem_init(&sem, 0, 2);

    // Init WS2812B
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    multicore_launch_core1(core1_entry);

    // Set up GPIO pins: Inputs first, then outputs
    for (int i = 0; i < 12; i++) {
        gpio_init(pinSwitch[i]);
        gpio_set_dir(pinSwitch[i], false);
        gpio_pull_up(pinSwitch[i]);
    }

    for (int i = 0; i < 10; i++) {
        gpio_init(pinLED[i]);
        gpio_set_dir(pinLED[i], true);
    }

    // init device stack on configured roothub port
    tud_init(BOARD_TUD_RHPORT);

    // Main loop
    while (1) {
        tud_task(); // tinyusb device task
        piuio_task();
    }

    return 0;
}
