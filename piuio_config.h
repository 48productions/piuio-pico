#ifndef PIUIO_PICO_PIUIO_CONFIG_H
#define PIUIO_PICO_PIUIO_CONFIG_H
#include "piuio_ws2812_helpers.h"

// Uncomment these defines to enable WS2812 LED support.
//#define ENABLE_WS2812_SUPPORT
//#define WS2812_IS_RGBW false
//#define WS2812_PIN 22

// Modify these to edit the colors of the cabinet lamps.
static uint32_t ws2812_color[5] = {
        urgb_u32(0, 255, 0),    // Lower left
        urgb_u32(255, 0, 0),    // Upper left
        urgb_u32(0, 0, 255),    // Bass / neon
        urgb_u32(255, 0, 0),    // Upper right
        urgb_u32(0, 255, 0)     // Lower right
};

// Modify these arrays to edit the pin out.
// Map these according to your button pins.
static const uint8_t pinSwitch[12] = {
        19,     // P1 DL
        21,     // P1 UL
        10,     // P1 CN
        6,      // P1 UR
        8,      // P1 DR
        17,     // P2 DL
        27,     // P2 UL
        2,      // P2 CN
        0,      // P2 UR
        4,      // P2 DR
        15,    // Service
        14     // Test
};

// Map these according to your LED pins.
static const uint8_t pinLED[10] = {
        18,     // P1 DL
        20,     // P1 UL
        11,     // P1 CN
        7,      // P1 UR
        9,      // P1 DR
        16,     // P2 DL
        26,     // P2 UL
        3,      // P2 CN
        1,      // P2 UR
        5       // P2 DR
};

#endif //PIUIO_PICO_PIUIO_CONFIG_H
