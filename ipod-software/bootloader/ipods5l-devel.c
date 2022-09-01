/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Dave Chapman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "config.h"

#include "inttypes.h"
#include "cpu.h"
#include "system.h"
#include "lcd.h"
#include "../kernel-internal.h"
#include "file_internal.h"
#include "storage.h"
#include "fat.h"
#include "disk.h"
#include "font.h"
#include "backlight.h"
#include "backlight-target.h"
#include "button.h"
#include "panic.h"
#include "power.h"
#include "file.h"
#include "common.h"
#include "rb-loader.h"
#include "loader_strerror.h"
#include "version.h"
#include "powermgmt.h"
#include "usb.h"
#ifdef HAVE_SERIAL
#include "serial.h"
#endif

#include "s5l8702.h"
#include "clocking-s5l8702.h"
#include "spi-s5l8702.h"
#include "i2c-s5l8702.h"
#include "gpio-s5l8702.h"
#include "pmu-target.h"
#include "norboot-target.h"

#define FW_ROCKBOX  0
#define FW_APPLE    1

#define ERR_RB  0
#define ERR_OF  1
#define ERR_HDD 2

/* Safety measure - maximum allowed firmware image size.
   The largest known current (October 2009) firmware is about 6.2MB so
   we set this to 8MB.
*/
#define MAX_LOADSIZE (8*1024*1024)

#define LCD_RBYELLOW    LCD_RGBPACK(255,192,0)
#define LCD_REDORANGE   LCD_RGBPACK(255,70,0)

extern void bss_init(void);
extern uint32_t _movestart;
extern uint32_t start_loc;

extern int line;

static int launch_onb(int clkdiv) {
    /* SPI clock = PClk/(clkdiv+1) */
    spi_clkdiv(SPI_PORT, clkdiv);

    /* Actually IRAM1_ORIG contains current RB bootloader IM3 header,
       it will be replaced by ONB IM3 header, so this function must
       be called once!!! */
    struct Im3Info *hinfo = (struct Im3Info*)IRAM1_ORIG;

    /* Loads ONB in IRAM0, exception vector table is destroyed !!! */
    int rc = im3_read(NORBOOT_OFF + im3_nor_sz(hinfo), hinfo, (void*)IRAM0_ORIG);

    if (rc != 0) {
        /* Restore exception vector table */
        memcpy((void*)IRAM0_ORIG, &_movestart, 4*(&start_loc-&_movestart));
        commit_discard_idcache();
        return rc;
    }

    /* Disable all external interrupts */
    eint_init();

    commit_discard_idcache();

    /* Branch to start of IRAM */
    asm volatile("mov pc, %0"::"r"(IRAM0_ORIG));
    while(1);
}

/* Launch OF when kernel mode is running */
static int kernel_launch_onb(void) {
    disable_irq();
    int rc = launch_onb(3); /* 54/4 = 13.5 MHz. */
    enable_irq();
    return rc;
}

/*  The boot sequence is executed on power-on or reset. After power-up
 *  the device could come from a state of hibernation, OF hibernates
 *  the iPod after an inactive period of ~30 minutes, on this state the
 *  SDRAM is in self-refresh mode.
 *
 *  t0 = 0
 *     S5L8702 BOOTROM loads an IM3 image located at NOR:
 *     - IM3 header (first 0x800 bytes) is loaded at IRAM1_ORIG
 *     - IM3 body (decrypted RB bootloader) is loaded at IRAM0_ORIG
 *     The time needed to load the RB bootloader (~100 Kb) is estimated
 *     on 200~250 ms. Once executed, RB booloader moves itself from
 *     IRAM0_ORIG to IRAM1_ORIG+0x800, preserving current IM3 header
 *     that contains the NOR offset where the ONB (original NOR boot),
 *     is located (see dualboot.c for details).
 *
 *  t1 = ~250 ms.
 *     If the PMU is hibernated, decrypted ONB (size 128Kb) is loaded
 *       and executed, it takes ~120 ms. Then the ONB restores the
 *       iPod to the state prior to hibernation.
 *     If not, initialize system and RB kernel, wait for t2.
 *
 *  t2 = ~650 ms.
 *     Check user button selection.
 *     If OF, diagmode, or diskmode is selected then launch ONB.
 *     If not, wait for LCD initialization.
 *
 *  t3 = ~700,~900 ms. (lcd_type_01,lcd_type_23)
 *     LCD is initialized, baclight ON.
 *     Wait for HDD spin-up.
 *
 *  t4 = ~2600,~2800 ms.
 *     HDD is ready.
 *     If hold switch is locked, then load and launch ONB.
 *     If not, load rockbox.ipod file from HDD.
 *
 *  t5 = ~2800,~3000 ms.
 *     rockbox.ipod is executed.
 */

#include "piezo.h"
#include "lcd-s5l8702.h"
extern int lcd_type;

static uint16_t alive[] = { 500,100,0, 0 };
static uint16_t alivelcd[] = { 2000,200,0, 0 };

static void run_of(void) {
    int tmo = 3;
    lcd_set_foreground(LCD_WHITE);
    while (tmo--) {
        printf("Booting to iPod in %d...", tmo);
        line--;
        sleep(HZ*1);
    }
    kernel_launch_onb();
}

void serial_write(const char* s) {
    for(int i = 0; i < strlen(s); i++) {
        tx_blocking_writec(s[i]);
    }
}

int serial_read(char* buf, int len) {
    int i = 0;
    while(i < len) {
        char c = rx_blocking_getc();
        if (c == '\r' || c > 0x7f) continue; // ignore CR or non-ascii because it's useless and some things just like to send it for fun
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = 0;
    return i;
}

uint32_t last_button;

int dev_menu() {
    lcd_clear_display();

    int focus = 0;
    int previous_focus = -1;
    int selection = -1;
    char* options[] = {"Back", "Send Reset Command", "Turn Off Serial", "Turn On Serial"};

    while(selection == -1) {
        line = 0;
            
        printf("Dev Menu:");
        printf("--------------------------");
        if(focus != previous_focus) {
            for(int i = 0; i < 4; i++) {
                if(i == focus) {
                    lcd_set_foreground(LCD_BLACK);
                    lcd_set_background(LCD_WHITE);
                    printf("-> %s", options[i]);
                } else {
                    lcd_set_foreground(LCD_WHITE);
                    lcd_set_background(LCD_BLACK);
                    printf("   %s", options[i]);
                }
            }
            previous_focus = focus;
        }

        switch(button_status()) {
            case BUTTON_LEFT:
                if(focus > 0 && last_button != BUTTON_LEFT) {
                    focus--;
                }
                last_button = BUTTON_LEFT;
                break;
            case BUTTON_RIGHT:
                if(focus < 3 && last_button != BUTTON_RIGHT) {
                    focus++;
                }
                last_button = BUTTON_RIGHT;
                break;
            case BUTTON_SELECT:
                if(last_button != BUTTON_SELECT) {
                    selection = focus;
                }
                last_button = BUTTON_SELECT;
                break;
            case BUTTON_MENU:
                selection = 0;
                break;
            default:
                last_button = 0;
                break;
        }
    }

    lcd_clear_display();
    return selection;
}

#define MAX_DEVICES 32
#define MAX_DEVICE_NAME 128

char devices[MAX_DEVICES][MAX_DEVICE_NAME];
int ndevices = 0;
char* selected_dev = NULL;
char focus = 0;
char serial_buf[1024];
int serialidx = 0;
bool update_devices = true;

void main(void) {
    #if 1
    int fw = FW_ROCKBOX;
    int rc = 0;

    usec_timer_init();

    piezo_seq(alive);

    /* Configure I2C0 */
    i2c_preinit(0);

    if (pmu_is_hibernated()) {
        fw = FW_APPLE;
        rc = launch_onb(1); /* 27/2 = 13.5 MHz. */
    }

    system_preinit();
    memory_init();
    /*
     * XXX: BSS is initialized here, do not use .bss before this line
     */
    bss_init();

    system_init();
    kernel_init();
    i2c_init();
    power_init();

    enable_irq();

#ifdef HAVE_SERIAL
    serial_setup();
#endif

    button_init();
    if (rc == 0) {
        /* User button selection timeout */
        while (USEC_TIMER < 400000);
        int btn = button_read_device();
        /* This prevents HDD spin-up when the user enters DFU */
        if (btn == (BUTTON_SELECT|BUTTON_MENU)) {
            while (button_read_device() == (BUTTON_SELECT|BUTTON_MENU))
                sleep(HZ/10);
            sleep(HZ);
            btn = button_read_device();
        }
        /* Enter OF, diagmode and diskmode using ONB */
        if ((btn == BUTTON_MENU)
                || (btn == (BUTTON_SELECT|BUTTON_LEFT))
                || (btn == (BUTTON_SELECT|BUTTON_PLAY))) {
            fw = FW_APPLE;
            rc = kernel_launch_onb();
        }
    }

    lcd_init();
    lcd_set_foreground(LCD_WHITE);
    lcd_set_background(LCD_BLACK);
    lcd_clear_display();
    font_init();
    lcd_setfont(FONT_SYSFIXED);

    sleep(HZ);
    for (int i = 0; i < lcd_type+1; i++) {
        sleep(HZ/2);
        piezo_seq(alivelcd);
    }

    lcd_update();
    sleep(HZ/40);  /* wait for lcd update */

    verbose = true;
    backlight_init(); /* Turns on the backlight */
    sleep(HZ);

    if(button_status() == BUTTON_PLAY) {
        printf("Disabling Serial Port");
        serial_stop();
        printf("Press Next To Re-enable and Continue");
        while(button_status() != BUTTON_RIGHT) {
            sleep(HZ/10);
        }
        serial_setup();
    }

    printf("Waiting for Bluetooth Board...");

    #endif

    // step 1: reset the ESP32 by sending "reset"
    serial_write("reset\n");

    memset(serial_buf, 0, 1024);
    memset(devices, 0, sizeof(devices));

    // step 2: wait for the ESP32 to send "starting"
    while(true) {
        memset(serial_buf, 0, 1024);
        serial_read(serial_buf, 1024);
        if(strncmp(serial_buf, "starting", 8) == 0) break;
        if(strlen(serial_buf) <= 1) continue;
        else serial_write("reset\n");
    }

    // step 3: the esp32 is going to continuously send device names. we add them to the set and then print them out.
    //         this is a hot loop - it will run until the user presses the select button and we advance to the next step.
    //         no serial comms will block in this loop.

    memset(serial_buf, 0, 1024);
    memset(devices, 0, sizeof(devices));
    lcd_clear_display();

    while(true) {
        while(rx_rdy()) {
            char c = rx_getc();
            if(c == '\n') {
                // we've received a newline - this means that the esp32 has finished sending us a device name.
                // we add 8 to the buffer pointer to chop off the "device: " prefix.
                // null terminate it
                serial_buf[serialidx] = 0;
                char* dev = serial_buf + 8;
                for(int i = 0; i < ndevices; i++) {
                    if(strcmp(devices[i], dev) == 0) goto already_exists;
                }

                // if we've reached the end of the array, ignore the message.
                if(ndevices == MAX_DEVICES) goto already_exists;

                // add the device to the array.
                memcpy(devices[ndevices++], dev, MAX_DEVICE_NAME);
                update_devices = true;

                already_exists:
                memset(serial_buf, 0, 1024);
                serialidx = 0;
            } else {
                serial_buf[serialidx++] = c;
            }
        }

        switch(button_status()) {
            case BUTTON_LEFT:
                if(focus > 0 && last_button != BUTTON_LEFT) {
                    focus--;
                }
                last_button = BUTTON_LEFT;
                update_devices = true;
                break;
            case BUTTON_RIGHT:
                if(focus < ndevices - 1 && last_button != BUTTON_RIGHT) {
                    focus++;
                }
                last_button = BUTTON_RIGHT;
                update_devices = true;
                break;
            case BUTTON_SELECT:
                if(last_button != BUTTON_SELECT && strlen(devices[focus]) > 1) {
                    selected_dev = devices[focus];
                }
                last_button = BUTTON_SELECT;
                break;
            case BUTTON_MENU:
                // emergency dev menu - offer reset and "temporarily turn off uart" so you can program the board in-circuit.
                switch (dev_menu()) {
                    case 0: // do nothing
                        break;
                    case 1: // reset
                        serial_write("reset\n");
                        break;
                    case 2: // turn off uart
                        serial_stop();
                        break;
                    case 3: // turn on uart
                        serial_setup();
                        break;

                }
            default:
                last_button = 0;
                break;
        }

        if(selected_dev) break;

        if(update_devices) {
            line = 0;
            
            printf("Select a device:");
            printf("--------------------------");
            for(int i = 0; i < ndevices; i++) {
                if(i == focus) {
                    lcd_set_foreground(LCD_BLACK);
                    lcd_set_background(LCD_WHITE);
                    printf("-> %s", devices[i]);
                } else {
                    lcd_set_foreground(LCD_WHITE);
                    lcd_set_background(LCD_BLACK);
                    printf("   %s", devices[i]);
                }
            }

            lcd_set_foreground(LCD_WHITE);
            lcd_set_background(LCD_BLACK);
            update_devices = false;
        }
    }

    // step 4: now that we know what to connect to, let's send the name back to the ESP32
    memset(serial_buf, 0, 1024);
    lcd_clear_display();
    line = focus + 1;
    printf("Connecting to");
    lcd_set_foreground(LCD_BLACK);
    lcd_set_background(LCD_WHITE);
    printf("-> %s", selected_dev);
    lcd_set_foreground(LCD_WHITE);
    lcd_set_background(LCD_BLACK);

    // serial_write("connect: ");
    serial_write(selected_dev);
    serial_write("\n");

    uint8_t dots = 0;

    // step 5: wait for the ESP32 to send "connected"
    while(true) {
        serial_read(serial_buf, 1024);
        if (strcmp(serial_buf, "connected") == 0)
            break;
        printf("%c", (dots % 4) == 0 ? '/' : (dots % 4) == 1 ? '-' : (dots % 4) == 2 ? '\\' : '|');
        line--;
        dots++;
    }

    printf("Connected!");

    // step 6: we are done talking to the esp32. we can now start the kernel.
    run_of();
}
