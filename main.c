
#include "bsp/board.h"
#include "hardware/gpio.h"
#include "device/usbd.h"

const uint8_t STATE_CAB_PLAYER_1 = 1;
const uint8_t STATE_PLAYER_1 = 0;
const uint8_t STATE_PLAYER_2 = 2;

// Set pins for the switches/LEDs
// Order of DL, UL, C, UR, DR for each player, then test and service switches
const uint8_t pinSwitch[12] = {19, 21, 10, 6, 8, 17, 27, 2, 0, 4, 15, 14};
const uint8_t pinLED[10] = {18, 20, 11, 7, 9, 16, 26, 3, 1, 5};
const uint8_t pinNEO = 22;

// PIUIO input and output data
uint8_t inputData[8];
uint8_t lampData[8];

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    // nothing to with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) return true;

    // Request 0xAE = IO Time
    if (request->bRequest == 0xAE) {
        switch (request->bmRequestType) {
            case 0x40:
                return tud_control_xfer(rhport, request, (void *)&lampData, 8);
            case 0xC0:
                return tud_control_xfer(rhport, request, (void *)&inputData, 8);
            default:
                return false;
        }
    }

    return false;
}

void piuio_task(void) {
    const uint8_t pos[] = { 3, 0, 2, 1, 4 };

    // P1 / P2
    for (int i = 0; i < 5; i++) {
        uint8_t* p1 = &inputData[STATE_PLAYER_1];
        uint8_t* p2 = &inputData[STATE_PLAYER_2];
        *p1 = gpio_get(pinSwitch[i]) ? tu_bit_set(*p1, pos[i]) : tu_bit_clear(*p1, pos[i]);
        *p2 = gpio_get(pinSwitch[i+5]) ? tu_bit_set(*p2, pos[i]) : tu_bit_clear(*p2, pos[i]);
    }

    // Test/Service
    inputData[STATE_CAB_PLAYER_1] = gpio_get(pinSwitch[10]) ? tu_bit_set(inputData[1], 1) : tu_bit_clear(inputData[1], 1);
    inputData[STATE_CAB_PLAYER_1] = gpio_get(pinSwitch[11]) ? tu_bit_set(inputData[1], 2) : tu_bit_clear(inputData[1], 2);
}

int main(void) {
    board_init();

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
