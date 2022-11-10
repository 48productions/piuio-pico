
#include "bsp/board.h"
#include "device/usbd.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "piuio_structs.h"
#include "piuio_config.h"

#ifdef ENABLE_WS2812_SUPPORT
#include "piuio_ws2812.h"
#endif

// Switch -> PIUIO input/output bytes mappings
// And the offset between the bit positions in the input byte and the bit positions in the output bytes for panel data
// (don't touch)
#ifdef ENABLE_BUTTON_BOARD // The mappings change between the piuio and button board
const uint8_t pos[] = { 1, 0, 3, 0, 2 };
const uint8_t lightsOffset = 0;
#else
const uint8_t pos[] = { 3, 0, 2, 1, 4 };
const uint8_t lightsOffset = 2;
#endif

// PIUIO input and output data
uint8_t inputData[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
struct lampArray lamp = {};

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

void piuio_task(void) {
    #ifdef ENABLE_WS2812_SUPPORT
    ws2812_lock_mtx();
    #endif

    // P1 / P2 inputs
    for (int i = 0; i < 5; i++) {
        uint8_t* p1 = &inputData[PLAYER_1];
        uint8_t* p2 = &inputData[PLAYER_2];
        *p1 = gpio_get(pinSwitch[i]) ? tu_bit_set(*p1, pos[i]) : tu_bit_clear(*p1, pos[i]);
        *p2 = gpio_get(pinSwitch[i+5]) ? tu_bit_set(*p2, pos[i]) : tu_bit_clear(*p2, pos[i]);
    }

    // Test/Service buttons
    inputData[CABINET] = gpio_get(pinSwitch[10]) ? tu_bit_set(inputData[1], 1) : tu_bit_clear(inputData[1], 1);
    inputData[CABINET] = gpio_get(pinSwitch[11]) ? tu_bit_set(inputData[1], 6) : tu_bit_clear(inputData[1], 6);

    // Write pad lamps
    for (int i = 0; i < 5; i++) {
        gpio_put(pinLED[i], tu_bit_test(lamp.data[PLAYER_1], pos[i] + lightsOffset));
        gpio_put(pinLED[i+5], tu_bit_test(lamp.data[PLAYER_2], pos[i] + lightsOffset));
    }

    // Write the bass neon to the onboard LED for testing + kicks
    gpio_put(25, lamp.bass_light);

    #ifdef ENABLE_WS2812_SUPPORT
    ws2812_unlock_mtx();
    #endif
}

int main(void) {
    board_init();

    // Init WS2812B
    #ifdef ENABLE_WS2812_SUPPORT
    ws2812_init(&lamp);
    #endif

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
    while (true) {
        tud_task(); // tinyusb device task
        piuio_task();
    }

    return 0;
}
