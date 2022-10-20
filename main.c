/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "hardware/gpio.h"
#include "device/usbd.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

// Set pins for the switches/LEDs
// Order of DL, UL, C, UR, DR for each player, then test and service switches
const uint8_t pinSwitch[12] = {19, 21, 10, 6, 8, 17, 27, 2, 0, 4, 15, 14};
const uint8_t pinLED[10] = {18, 20, 11, 7, 9, 16, 26, 3, 1, 5};
const uint8_t pinNEO = 22;

// PIUIO input and output data
uint8_t inputData[8];
uint8_t lampData[8];

void led_blinking_task(void);
void hid_task(void);

// Renamed from webusb_task, do we actually need this?
void piuio_task(void)
{
    // Read our switch inputs into the game-ready inputData array
    // P1

    gpio_put(25, gpio_get(pinSwitch[0]));

    bool input = gpio_get(pinSwitch[0]); if (input) { inputData[0] = tu_bit_set(inputData[0], 3); } else { inputData[0] = tu_bit_clear(inputData[0], 3); }
    input = gpio_get(pinSwitch[1]); if (input) { inputData[0] = tu_bit_set(inputData[0], 0); } else { inputData[0] = tu_bit_clear(inputData[0], 0); }
    input = gpio_get(pinSwitch[2]); if (input) { inputData[0] = tu_bit_set(inputData[0], 2); } else { inputData[0] = tu_bit_clear(inputData[0], 2); }
    input = gpio_get(pinSwitch[3]); if (input) { inputData[0] = tu_bit_set(inputData[0], 1); } else { inputData[0] = tu_bit_clear(inputData[0], 1); }
    input = gpio_get(pinSwitch[4]); if (input) { inputData[0] = tu_bit_set(inputData[0], 4); } else { inputData[0] = tu_bit_clear(inputData[0], 4); }

    // P2
    input = gpio_get(pinSwitch[5]); if (input) { inputData[2] = tu_bit_set(inputData[2], 3); } else { inputData[2] = tu_bit_clear(inputData[2], 3); }
    input = gpio_get(pinSwitch[6]); if (input) { inputData[2] = tu_bit_set(inputData[2], 0); } else { inputData[2] = tu_bit_clear(inputData[2], 0); }
    input = gpio_get(pinSwitch[7]); if (input) { inputData[2] = tu_bit_set(inputData[2], 2); } else { inputData[2] = tu_bit_clear(inputData[2], 2); }
    input = gpio_get(pinSwitch[8]); if (input) { inputData[2] = tu_bit_set(inputData[2], 1); } else { inputData[2] = tu_bit_clear(inputData[2], 1); }
    input = gpio_get(pinSwitch[9]); if (input) { inputData[2] = tu_bit_set(inputData[2], 4); } else { inputData[2] = tu_bit_clear(inputData[2], 4); }

    // Test/Service
    input = gpio_get(pinSwitch[10]); if (input) { inputData[1] = tu_bit_set(inputData[1], 1); } else { inputData[1] = tu_bit_clear(inputData[1], 1); }
    input = gpio_get(pinSwitch[11]); if (input) { inputData[1] = tu_bit_set(inputData[1], 2); } else { inputData[1] = tu_bit_clear(inputData[1], 2); }

    // Check if we've received... data..?
    if ( tud_vendor_available() )
    {
        uint8_t buf[64];
        uint32_t count = tud_vendor_read(buf, sizeof(buf));

        // echo back to both web serial and cdc
        //echo_all(buf, count);
    }

}


/*------------- MAIN -------------*/
int main(void)
{
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
    while (1)
    {
        tud_task(); // tinyusb device task
        piuio_task();
    }

    return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

