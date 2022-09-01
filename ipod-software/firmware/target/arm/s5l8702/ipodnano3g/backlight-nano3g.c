/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: backlight-nano2g.c 28601 2010-11-14 20:39:18Z theseven $
 *
 * Copyright (C) 2009 by Dave Chapman
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
#include <stdbool.h>

#include "config.h"
#include "kernel.h"
#include "backlight.h"
#include "backlight-target.h"
#include "pmu-target.h"

#ifdef HAVE_LCD_SLEEP
#include "lcd.h"
#include "lcd-s5l8702.h"
// bool lcd_active(void);
// void lcd_awake(void);
// void lcd_update(void);
#endif

// TODO: test
void backlight_hw_brightness(int brightness)
{
//     pmu_write(0x28, brightness);
    pmu_write(D1671_REG_LEDCTL,
        //(pmu_read(D1671_REG_LEDCTL) & ~D1671_LEDCTL_OUT_MASK) | brightness);
        //(pmu_read(D1671_REG_LEDCTL) & ~D1671_LEDCTL_OUT_MASK) | brightness | 0x40);
        (pmu_read(D1671_REG_LEDCTL) & ~D1671_LEDCTL_OUT_MASK) | (brightness>>1));
        //(pmu_read(D1671_REG_LEDCTL) & ~D1671_LEDCTL_OUT_MASK) | (brightness>>1) | 0x40);

// 3G                          6G 
// 0x00        0000 0000       0x00
// 0xc8        1100 1000       0x01
// 0xd3        1101 0011       0x10     0001 0000
// 0xdf        1101 1111       0x1e     0001 1110
    //pmu_write(D1671_REG_LEDCTL, 0xdf);    // imagen congelada, mucho brillo
                                            // 1101 1111
    //pmu_write(D1671_REG_LEDCTL, 0x9f);      // idem
                                            // 1001 1111
//     pmu_write(D1671_REG_LEDCTL, 0xaf);      // mejor pero poco brillo
                                            // 1010 1111
//     pmu_write(D1671_REG_LEDCTL, 0xbf);      // ahora brillo a tope!
                                            // 1011 1111                                            <--- XXX: la unica que va bien sin LCD congelado
                                            // bit 5 podria tener que estar a 1 para descongelar (?)
//     pmu_write(D1671_REG_LEDCTL, 0x3f);      // brillo a 0
                                            // 0011 1111
                                            // bit 7 podria ser brillo
//     pmu_write(D1671_REG_LEDCTL, 0xff);      // brillo a tope pero imagen congelada
                                            // 1111 1111
//     pmu_write(D1671_REG_LEDCTL, 0x1f);      // brillo a 0
                                            // 0001 1111
//      pmu_write(D1671_REG_LEDCTL, 0xd3); // XXX: OK, brillo medio
//      pmu_write(D1671_REG_LEDCTL, 0xdf); // XXX: OK, brillo a tope
//      pmu_write(D1671_REG_LEDCTL, 0x9f); // XXX: OK, menos brillo que df
//      pmu_write(D1671_REG_LEDCTL, 0xa0); // XXX: OK, menos brillo que 0x9f !!! bastante menos !!!
//      pmu_write(D1671_REG_LEDCTL, 0xc0); // XXX: OK, muy parecido si no el mismo que 0xa0
//      pmu_write(D1671_REG_LEDCTL, 0x80); // XXX: OK, muy parecido si no el mismo que 0xa0 !!!, igual el bit 5 no hace nada (de brillo) mirar igual FADE                                                              //                                                    XXX XXX XXX: el bit 6 podria ser un magnificador, y el 7 el on/off
//      pmu_write(D1671_REG_LEDCTL, 0xff); // OK a tope
//      pmu_write(D1671_REG_LEDCTL, 0xbf);
//      pmu_write(D1671_REG_LEDCTL, 0x9f);     // XXX: muy pareceido a bf (si no igual) pero menos que df o 0xff
//      pmu_write(D1671_REG_LEDCTL, 0x7f);     // XXX: no se ve nada

//      pmu_write(D1671_REG_LEDCTL, 0xbf);
}

void backlight_hw_on(void)
{
#ifdef HAVE_LCD_SLEEP
    if (!lcd_active())
    #if 1
        lcd_awake();
    #else
    {
        lcd_awake();
        lcd_update();
        lcd_update();   // XXX: asegura que se ha actualizado el lcd la primera vez
        //sleep(HZ/20);
    }
    #endif       
#endif
// //     pmu_write(0x29, 1);
    pmu_write(D1671_REG_LEDCTL,
            (pmu_read(D1671_REG_LEDCTL) | D1671_LEDCTL_ENABLE));
}

void backlight_hw_off(void)
{
// //     pmu_write(0x29, 0);
    pmu_write(D1671_REG_LEDCTL,
            (pmu_read(D1671_REG_LEDCTL) & ~D1671_LEDCTL_ENABLE));
}

bool backlight_hw_init(void)
{
//     /* LEDCTL: overvoltage protection enabled, OCP limit is 500mA */
//     pmu_write(0x2a, 0x05);
//     pmu_write(0x2b, 0x14);  /* T_dimstep = 16 * value / 32768 */
    backlight_hw_brightness(DEFAULT_BRIGHTNESS_SETTING);
    backlight_hw_on();
    return true;
}

/* Kill the backlight, instantly. */
void backlight_hw_kill(void)
{
//     pmu_write(0x2b, 0);  /* T_dimstep = 0 */
    backlight_hw_off();
}
