/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
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
#include "config.h"
#include "cpu.h"
#include <stdbool.h>
#include "kernel.h"
#include "system.h"
#include "power.h"
#include "logf.h"
#include "pcf50605.h"
#include "usb.h"
#include "lcd.h"
#include "string.h"
#ifdef HAVE_USB_CHARGING_ENABLE
#include "usb_core.h"   /* for usb_charging_maxcurrent_change */
#endif

void power_init(void)
{
#if defined(IPOD_1G2G) || defined(IPOD_3G)
    GPIOC_ENABLE |= 0x40;      /* GPIO C6 is HDD power (low active) */
    GPIOC_OUTPUT_VAL &= ~0x40; /* on by default */
    GPIOC_OUTPUT_EN |= 0x40;   /* enable output */
#endif
#ifndef IPOD_1G2G
    pcf50605_init();
#endif
}

#if CONFIG_CHARGING
unsigned int power_input_status(void)
{
    unsigned int status = POWER_INPUT_NONE;

#if defined(IPOD_NANO) || defined(IPOD_VIDEO)
    if ((GPIOL_INPUT_VAL & 0x08) == 0)
        status = POWER_INPUT_MAIN_CHARGER;

    if ((GPIOL_INPUT_VAL & 0x10) != 0)
        status |= POWER_INPUT_USB_CHARGER;
    /* */
#elif defined(IPOD_4G) || defined(IPOD_COLOR) \
       || defined(IPOD_MINI) || defined(IPOD_MINI2G)
    /* C2 is firewire power */
    if ((GPIOC_INPUT_VAL & 0x04) == 0)
        status = POWER_INPUT_MAIN_CHARGER;

    if ((GPIOD_INPUT_VAL & 0x08) != 0)
        status |= POWER_INPUT_USB_CHARGER;
    /* */
#elif defined(IPOD_3G)
    /* firewire power */
    if ((GPIOC_INPUT_VAL & 0x10) == 0)
        status = POWER_INPUT_MAIN_CHARGER;
    /* */
#else
    /* This needs filling in for other ipods. */
#endif

    return status;
}

/* Returns true if the unit is charging the batteries. */
bool charging_state(void) {
#if defined(IPOD_COLOR)
    /* 0x70000088 appears to be the input value for GPO32 bits.
       Write a zero to 0x70000088 before reading.
       To enable input set the corresponding 0x7000008C bit,
       and clear the corresponding GPO32_ENABLE bit. */
    outl(0, 0x70000088);
    return (inl(0x70000088) & 1)?false:true;
#else
    return (GPIOB_INPUT_VAL & 0x01)?false:true;
#endif
}
#endif /* CONFIG_CHARGING */


void ide_power_enable(bool on)
{
#if defined(IPOD_1G2G) || defined(IPOD_3G)
    if (on)
        GPIOC_OUTPUT_VAL &= ~0x40;
    else
        GPIOC_OUTPUT_VAL |= 0x40;
#elif defined(IPOD_4G) || defined(IPOD_COLOR) \
   || defined(IPOD_MINI) || defined(IPOD_MINI2G)
    if (on)
    {
        GPIO_CLEAR_BITWISE(GPIOJ_OUTPUT_VAL, 0x04);
        DEV_EN |= DEV_IDE0;
    }
    else
    {
        DEV_EN &= ~DEV_IDE0;
        GPIO_SET_BITWISE(GPIOJ_OUTPUT_VAL, 0x04);
    }
#elif defined(IPOD_VIDEO)
    if (on)
    {
        GPO32_VAL &= ~0x40000000;
        sleep(1);  /* only need 4 ms */
        DEV_EN |= DEV_IDE0;
        GPIOG_ENABLE = 0;
        GPIOH_ENABLE = 0;
        GPIO_CLEAR_BITWISE(GPIOI_ENABLE, 0xBF);
        GPIO_CLEAR_BITWISE(GPIOK_ENABLE, 0x1F);
        udelay(10);
    }
    else
    {
        DEV_EN &= ~DEV_IDE0;
        udelay(10);
        GPIOG_ENABLE = 0xFF;
        GPIOH_ENABLE = 0xFF;
        GPIO_SET_BITWISE(GPIOI_ENABLE, 0xBF);
        GPIO_SET_BITWISE(GPIOK_ENABLE, 0x1F);
        GPO32_VAL |= 0x40000000;
    }
#else /* Nano */
    (void)on;  /* Do nothing. */
#endif
}

bool ide_powered(void)
{
#if defined(IPOD_1G2G) || defined(IPOD_3G)
    return !(GPIOC_OUTPUT_VAL & 0x40);
#elif defined(IPOD_4G) || defined(IPOD_COLOR) \
   || defined(IPOD_MINI) || defined(IPOD_MINI2G)
    return !(GPIOJ_OUTPUT_VAL & 0x04);
#elif defined(IPOD_VIDEO)
    return !(GPO32_VAL & 0x40000000);
#else /* Nano */
    return true; /* Pretend we are always powered */
#endif
}

void power_off(void)
{
#if defined(HAVE_LCD_COLOR) && !defined(HAVE_LCD_SHUTDOWN)
    /* Clear the screen and backdrop to
    remove ghosting effect on shutdown */
    lcd_set_backdrop(NULL);
    lcd_set_background(LCD_WHITE);
    lcd_clear_display();
    lcd_update();
    sleep(HZ/16);
#endif

#ifndef BOOTLOADER
#ifdef IPOD_1G2G
    /* we cannot turn off the 1st gen/ 2nd gen yet. Need to figure out sleep mode. */
    system_reboot();
#else
    /* We don't turn off the ipod, we put it in a deep sleep */
    /* Clear latter part of iram (the part used by plugins/codecs) to ensure
     * that the OF behaves properly on boot. There is some kind of boot
     * failure flag there which otherwise may not be cleared.
     */
#if CONFIG_CPU == PP5022
    memset((void*)0x4000c000, 0, 0x14000);
#elif CONFIG_CPU == PP5020
    memset((void*)0x4000c000, 0, 0xc000);
#endif
    pcf50605_standby_mode();
#endif
#endif
}

#ifdef HAVE_USB_CHARGING_ENABLE
void usb_charging_maxcurrent_change(int maxcurrent)
{
    bool suspend_charging = (maxcurrent < 100);
    bool fast_charging = (maxcurrent >= 500);

    /* This GPIO is connected to the LTC4066's SUSP pin */
    /* Setting it high prevents any power being drawn over USB */
    /* which supports USB suspend */
#if defined(IPOD_VIDEO) || defined(IPOD_NANO)
    if (suspend_charging)
        GPIO_SET_BITWISE(GPIOL_OUTPUT_VAL, 4);
    else
        GPIO_CLEAR_BITWISE(GPIOL_OUTPUT_VAL, 4);
#elif defined(IPOD_MINI2G)
    if (suspend_charging)
        GPIO_SET_BITWISE(GPIOJ_OUTPUT_VAL, 2);
    else
        GPIO_CLEAR_BITWISE(GPIOJ_OUTPUT_VAL, 2);
#else /* Color, 4G, Mini G1 */
    if (suspend_charging)
        GPO32_VAL |= 0x8000000;
    else
        GPO32_VAL &= ~0x8000000;
#endif

    /* This GPIO is connected to the LTC4066's HPWR pin */
    /* Setting it low limits current to 100mA, setting it high allows 500mA */
#if defined(IPOD_VIDEO) || defined(IPOD_NANO)
    if (fast_charging)
        GPIO_SET_BITWISE(GPIOA_OUTPUT_VAL, 4);
    else
        GPIO_CLEAR_BITWISE(GPIOA_OUTPUT_VAL, 4);
#else
    if (fast_charging)
        GPO32_VAL |= 0x40;
    else 
        GPO32_VAL &= ~0x40;
#endif

    /* This GPIO is connected to the LTC4066's CLDIS pin */
    /* Setting it high allows up to 1.5A of current to be drawn */
    /* This doesn't appear to actually be safe even with an AC charger */
    /* so for now it is disabled. It's not known (or maybe doesn't exist) */
    /* on all models. */
#if 0
#if defined(IPOD_VIDEO)
    if (unlimited_charging)
        GPO32_VAL |= 0x10000000;
    else 
        GPO32_VAL &= ~0x10000000;
#elif defined(IPOD_4G) || defined(IPOD_COLOR)
    if (unlimited_charging)
        GPO32_VAL |= 0x200;
    else 
        GPO32_VAL &= ~0x200;
#endif
    /* This might be GPIOD & 40 on 2G */
#endif
}
#endif /* HAVE_USB_CHARGING_ENABLE */
