/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:
 *
 * Copyright © 2009 Michael Sparmann
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
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "s5l8702.h"
#include "clocking-s5l8702.h"
#include "spi-s5l8702.h"

#define SPI_N_PORT 3

static int clkdiv[SPI_N_PORT] = {4, 4, 4};

/* configure SPI clock, speed is PClk/(div+1) (TBC) */
void spi_clkdiv(int port, int div)
{
    clkdiv[port] = div;
}

/* state: 1 -> route GPIO ports to SPI controller
 *        0 -> set GPIO to lowest power consumption
 */
void spi_init(int port, bool state)
{
    uint32_t val = state ? 0x2222 : 0xEEEF;
    switch (port)
    {
        case 0:
            PCON0 = (PCON0 & ~0xffff) | val;
            break;
#if CONFIG_CPU == S5L8702
        case 1:
            PCON6 = (PCON6 & ~0xffff0000) | (val << 16);
            break;
        case 2:
            PCONE = (PCONE & ~0xff000000) | (val << 24);
            PCONF = (PCONF & ~0xff) | (val >> 8);
            break;
#else /* S5L8720 */
        case 1:
            PCON4 = (PCON4 & ~0xff000000) | (val << 24);
            PCON5 = (PCON5 & ~0xff) | (val >> 8);
            break;
        case 2:
            /* unknown */
            break;
#endif
    }
}

/* nSS pin: output LOW -> external device SLAVE
 *          output HIGH -> deactivate external SLAVE device
 *          input LOW -> SLAVE condition (external MASTER device)
 */
void spi_ce(int port, bool state)
{
    uint32_t level = state ? 0 : 1;
    switch (port)
    {
        case 0: GPIOCMD = 0x0000e | level; break;
#if CONFIG_CPU == S5L8702
        case 1: GPIOCMD = 0x6040e | level; break;
        case 2: GPIOCMD = 0xe060e | level; break;
#else /* S5L8720 */
        case 1: GPIOCMD = 0x4060e | level; break;
        case 2: /* unknown */ break;
#endif
    }
}

void spi_prepare(int port)
{
    clockgate_enable(SPICLKGATE(port), true);
#if CONFIG_CPU == S5L8720
    clockgate_enable(SPICLKGATE_2(port), true);
#endif    
    SPISTATUS(port) = 0xf;
    SPICTRL(port) |= 0xc;
    SPICLKDIV(port) = clkdiv[port];
    SPIPIN(port) = 6;
#if CONFIG_CPU == S5L8702
    SPISETUP(port) = 0x10618;
#else /* S5L8720 */
    SPIUNK40(port) = 0xffffffff;
    SPIUNK3C(port) = 0xffffffff;
    SPISETUP(port) = 0x40618;
#endif    
    SPICTRL(port) |= 0xc;
    SPICTRL(port) = 1;
}

void spi_release(int port)
{
    clockgate_enable(SPICLKGATE(port), false);
#if CONFIG_CPU == S5L8720
    clockgate_enable(SPICLKGATE_2(port), false);
#endif    
}

#if 1
static inline void spi_wait_ready(int port)
{
#if CONFIG_CPU == S5L8702
    while (!(SPISTATUS(port) & 0x3e00));
#else /* S5L8720 */
    while (!(SPISTATUS(port) & 0xf800));
#endif
}    

uint32_t spi_write(int port, uint32_t data)
{
#if CONFIG_CPU == S5L8702
    SPIRXLIMIT(port) = 1;
    while ((SPISTATUS(port) & 0x1f0) == 0x100);
#else /* S5L8720 */
    SPIUNK4C(port) = 1;
    SPIRXLIMIT(port) = 1;
    while ((SPISTATUS(port) & 0x7f0) == 0x400);         // TODO: XXX: esto no seria 0x7c0 ??? para que se cumpla la regla de <<2 en este registro ???
#endif
    SPITXDATA(port) = data;
    spi_wait_ready(port);
    return SPIRXDATA(port);
}

void spi_read(int port, uint32_t size, void* buf)
{
    uint8_t* buffer = (uint8_t*)buf;
#if CONFIG_CPU == S5L8720
    SPIUNK4C(port) = size;
#endif    
    SPIRXLIMIT(port) = size;
    SPISETUP(port) |= 1;
    while (size--)
    {
        spi_wait_ready(port);
        *buffer++ = SPIRXDATA(port);
    }
    SPISETUP(port) &= ~1;
}


#else
uint32_t spi_write(int port, uint32_t data)
{
#if CONFIG_CPU == S5L8702
    SPIRXLIMIT(port) = 1;
    while ((SPISTATUS(port) & 0x1f0) == 0x100);
    SPITXDATA(port) = data;
    while (!(SPISTATUS(port) & 0x3e00));
#else /* S5L8720 */
    SPIUNK4C(port) = 1;
    SPIRXLIMIT(port) = 1;
    while ((SPISTATUS(port) & 0x7f0) == 0x400);
    SPITXDATA(port) = data;
    while (!(SPISTATUS(port) & 0xf800));
#endif
    return SPIRXDATA(port);
}

void spi_read(int port, uint32_t size, void* buf)
{
    uint8_t* buffer = (uint8_t*)buf;
#if CONFIG_CPU == S5L8720
    SPIUNK4C(port) = size;
#endif    
    SPIRXLIMIT(port) = size;
    SPISETUP(port) |= 1;
    while (size--)
    {
#if CONFIG_CPU == S5L8702
        while (!(SPISTATUS(port) & 0x3e00));
#else /* S5L8720 */
        while (!(SPISTATUS(port) & 0xf800));
#endif        
        *buffer++ = SPIRXDATA(port);
    }
    SPISETUP(port) &= ~1;
}
#endif
