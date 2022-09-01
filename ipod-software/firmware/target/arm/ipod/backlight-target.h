/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 by Barry Wardell
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
#ifndef BACKLIGHT_TARGET_H
#define BACKLIGHT_TARGET_H

#if defined(IPOD_VIDEO) || defined(IPOD_NANO)

bool backlight_hw_init(void);
void backlight_hw_brightness(int val);
void _backlight_led_on(void);
void _backlight_led_off(void);
void _backlight_hw_enable(bool on);

#ifdef HAVE_LCD_SLEEP
void lcd_awake(void);
#endif

#ifdef BOOTLOADER
#define backlight_hw_on()  do { _backlight_hw_enable(true); \
                              _backlight_led_on(); } while(0)
#define backlight_hw_off() do { _backlight_led_off(); \
                              _backlight_hw_enable(false); } while(0)
#else /* !BOOTLOADER */
#define _backlight_on_isr() _backlight_led_on()
#define _backlight_off_isr() _backlight_led_off()
#define _backlight_on_normal()  do { _backlight_hw_enable(true); \
                                     _backlight_led_on(); } while(0)
#define _backlight_off_normal() do { _backlight_led_off(); \
                                     _backlight_hw_enable(false); } while(0)
#define _BACKLIGHT_FADE_ENABLE
#endif /* !BOOTLOADER */

#elif defined(IPOD_4G) || defined(IPOD_COLOR)

bool backlight_hw_init(void);
void backlight_hw_on(void);
void backlight_hw_off(void);
void backlight_hw_brightness(int val);

#elif defined(IPOD_MINI) || defined(IPOD_MINI2G)

#define backlight_hw_init() true
void backlight_hw_on(void);
void backlight_hw_off(void);

#ifndef BOOTLOADER
#define _backlight_on_isr() backlight_hw_on()
#define _backlight_off_isr() backlight_hw_off()
#define _backlight_on_normal() backlight_hw_on()
#define _backlight_off_normal() backlight_hw_off()
#endif

#elif defined(IPOD_1G2G) || defined(IPOD_3G)

#define backlight_hw_init() true
void backlight_hw_on(void);
void backlight_hw_off(void);
#endif

#endif
