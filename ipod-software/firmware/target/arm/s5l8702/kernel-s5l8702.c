/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: kernel-s5l8700.c 28795 2010-12-11 17:52:52Z Buschel $
 *
 * Copyright � 2009 Bertrik Sikken
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
#include "system.h"
#include "kernel.h"

/*  S5L8702 driver for the kernel timer

    Timer B is configured as a 10 kHz timer
 */

void INT_TIMERB(void)
{
    /* clear interrupt */
    TBCON = TBCON;

// static uint16_t alive3[] = { 1000,2000,0, 0 };
// piezo_seq(alive3);
// udelay(2000000);

    call_tick_tasks();  /* Run through the list of tick tasks */
}

void tick_start(unsigned int interval_in_ms)
{
    int cycles = 10 * interval_in_ms;
    
#if CONFIG_CPU == S5L8702    
    /* configure timer for 10 kHz (12 MHz / 16 / 75) */
    TBCMD = (1 << 1);   /* TB_CLR */    
    TBPRE = 75 - 1;     /* prescaler */
    TBCON = (0 << 13) | /* TB_INT1_EN */
            (1 << 12) | /* TB_INT0_EN */
            (0 << 11) | /* TB_START */
            (2 << 8) |  /* TB_CS = ECLK / 16 */
            (1 << 6) |  /* select ECLK (12 MHz) */
            (0 << 4);   /* TB_MODE_SEL = interval mode */
    TBDATA0 = cycles;   /* set interval period */
    TBCMD = (1 << 0);   /* TB_EN */

#else /* S5L8720 */
    /* configure timer for 10 kHz (24 MHz / 16 / 150) */
    TBCMD = (1 << 1);   /* TB_CLR */    
    TBPRE = 150 - 1;    /* prescaler */
    TBCON = (0 << 13) | /* TB_INT1_EN */
            (1 << 12) | /* TB_INT0_EN */
            (0 << 11) | /* TB_START */
            (2 << 8) |  /* TB_CS = ECLK / 16 */
            (1 << 6) |  /* select ECLK (24 MHz) */
            (0 << 4);   /* TB_MODE_SEL = interval mode */
    TBDATA0 = cycles;   /* set interval period */
    TBCMD = (1 << 0);   /* TB_EN */
#endif    
}

