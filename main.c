
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
// And the offset between the index of the pinSwitch array and the bit position in the input byte to map that input pin to
// (don't touch)
#ifdef ENABLE_BUTTON_BOARD // The mappings change between the piuio and button board
const uint8_t pos[]      = { 1, 0, 3, 0, 2 }; // Map L, O, S, O, R -> O, L, R, S
const uint8_t lightpos[] = { 2, 3, 0, 3, 1 }; // Map L, O, S, O, R -> S, L, R, O
#else
const uint8_t pos[] = { 3, 0, 2, 1, 4 }; // Map DL, UL, C, UR, DR -> UL, UR, C, DL, DR
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
    #ifdef ENABLE_BUTTON_BOARD // Button board only uses 1 byte for all the inputs
    for (int i = 0; i < 5; i++) {
        uint8_t* btn = &inputData[PLAYER_1];
		// Extra logic - UL/UR should both map to select (for LX-style or PicoFX-based button boards)
		// If we're reading UR (input index 3), skip reading if UL was found to be pressed. This effectively ORs the inputs together
		if (i != 3 || tu_bit_test(*btn, 0)) {
			*btn = gpio_get(pinSwitch[i]) ? tu_bit_set(*btn, pos[i]) : tu_bit_clear(*btn, pos[i]);
		}
		if (i != 3 || tu_bit_test(*btn, 4)) {
			*btn = gpio_get(pinSwitch[i+5]) ? tu_bit_set(*btn, pos[i]+4) : tu_bit_clear(*btn, pos[i]+4);
		}
    }
    
    #else // For the pad IO, read inputs into two bytes, and read the test/service switches
    for (int i = 0; i < 5; i++) {
        uint8_t* p1 = &inputData[PLAYER_1];
        uint8_t* p2 = &inputData[PLAYER_2];
        *p1 = gpio_get(pinSwitch[i]) ? tu_bit_set(*p1, pos[i]) : tu_bit_clear(*p1, pos[i]);
        *p2 = gpio_get(pinSwitch[i+5]) ? tu_bit_set(*p2, pos[i]) : tu_bit_clear(*p2, pos[i]);
    }

    // Test/Service buttons
    inputData[CABINET] = gpio_get(pinSwitch[10]) ? tu_bit_set(inputData[1], 1) : tu_bit_clear(inputData[1], 1);
    inputData[CABINET] = gpio_get(pinSwitch[11]) ? tu_bit_set(inputData[1], 6) : tu_bit_clear(inputData[1], 6);
    #endif


    // Write pad/button lamps
	#ifdef ENABLE_BUTTON_BOARD // Button board only uses 1 byte for outputs
	for (int i = 0; i < 5; i++) {
		gpio_put(pinLED[i], tu_bit_test(lamp.data[PLAYER_1], lightpos[i]+4));
		gpio_put(pinLED[i+5], tu_bit_test(lamp.data[PLAYER_1], lightpos[i]));
	}
	
	#else // Pad IO: Use both bytes
	for (int i = 0; i < 5; i++) {
		gpio_put(pinLED[i], tu_bit_test(lamp.data[PLAYER_1], pos[i]+2));
		gpio_put(pinLED[i+5], tu_bit_test(lamp.data[PLAYER_2], pos[i]+2));
	}
	#endif
	
	

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
