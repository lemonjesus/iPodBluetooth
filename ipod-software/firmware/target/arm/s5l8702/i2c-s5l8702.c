/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: i2c-s5l8700.c 28589 2010-11-14 15:19:30Z theseven $
 *
 * Copyright (C) 2009 by Bertrik Sikken
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
#include "i2c-s5l8702.h"
#include "clocking-s5l8702.h"

/*  Driver for the s5l8702 built-in I2C controller in master mode

    Both the i2c_read and i2c_write function take the following arguments:
    * slave, the address of the i2c slave device to read from / write to
    * address, optional sub-address in the i2c slave (unused if -1)
    * len, number of bytes to be transfered
    * data, pointer to data to be transfered
    A return value > 0 indicates an error.

    Note:
    * blocks the calling thread for the entire duraton of the i2c transfer.
*/

static struct mutex i2c_mtx[2];

void i2c_init()
{
    mutex_init(&i2c_mtx[0]);
    mutex_init(&i2c_mtx[1]);
}

// int caller=1;

#if 0
static void wait_rdy(int bus)
{
    uint32_t tmo = USEC_TIMER + 20000;
    while (IICUNK10(bus))
         if (USEC_TIMER > tmo)
            panicf("wait_rdy()");
}
#else
static void wait_rdy(int bus)
{
    while (IICUNK10(bus));
}
#endif

static void i2c_on(int bus)
{
    /* enable I2C clock */
    clockgate_enable(I2CCLKGATE(bus), true);
#if CONFIG_CPU == S5L8720
    /* this clockgate is needed when ECLK is used as source clock */
    clockgate_enable(I2CCLKGATE_2(bus), true);
// udelay(100);
// udelay(50);
// udelay(20);
// udelay(10);
udelay(5);
#endif
// udelay(100);
}

static void i2c_off(int bus)
{
    /* serial output off */
    wait_rdy(bus);
    IICSTAT(bus) = 0;
    /* disable I2C clock */
    wait_rdy(bus);
    clockgate_enable(I2CCLKGATE(bus), false);
#if CONFIG_CPU == S5L8720
    clockgate_enable(I2CCLKGATE_2(bus), false);
#endif
}

/* wait for bus not busy, or tx/rx byte (should return once
   8 data + 1 ack clocks are generated), or STOP. */
// #if CONFIG_CPU == S5L8702
#if 0
static void i2c_wait_io(int bus)
{
// uint32_t tmo = USEC_TIMER + 1000000;
// wait_rdy(bus);

    while (((IICSTAT(bus) & (1 << 5)) != 0) &&
            ((IICSTA2(bus) & ((1 << 8)|(1 << 13))) == 0))
//     {
        wait_rdy(bus);
// if (USEC_TIMER > tmo) panicf("i2c_wait_io() %d %d %d", i2c_err[0]-1, i2c_err[1]-1, i2c_err[2]-1);
// if (USEC_TIMER > tmo) panicf("i2c_wait_io() %d %d %d - %d", i2c_err[0]-1, i2c_err[1]-1, i2c_err[2]-1, caller);
// if (USEC_TIMER > tmo) panicf("i2c_wait_io() %d", caller);
//     }
    IICSTA2(bus) |= (1 << 8)|(1 << 13);
}
#else /* S5L8720 */

static void i2c_wait_io(int bus)
{
// uint32_t tmo = USEC_TIMER + 1000000;

    while (((IICSTAT(bus) & (1 << 5)) != 0) &&
            ((IICSTA2(bus) & ((1 << 8)|(1 << 13))) == 0));
// {
// if (USEC_TIMER > tmo) panicf("i2c_wait_io() %d %d %d - %d", i2c_err[0]-1, i2c_err[1]-1, i2c_err[2]-1, caller);
// if (USEC_TIMER > tmo) panicf("i2c_wait_io() %d", caller);
// }

    wait_rdy(bus);
    IICSTA2(bus) |= (1 << 8)|(1 << 13);
}
#endif

static int i2c_start(int bus, unsigned char slave, bool rd)
{
    /* configure port */
    wait_rdy(bus);
    IICCON(bus) = (0 << 8) | /* INT_EN = disabled */
                  (1 << 7) | /* ACK_GEN */
                  (0 << 6) | /* CLKSEL = SRCCLK/32 (TBC) */
#if CONFIG_CPU == S5L8702
//                   (4 << 0);  /* CK_REG */
(0 << 0);  /* CK_REG */
#else /* S5L8720 */
                  (1 << 0);  /* CK_REG */
// (4 << 0);  /* CK_REG */
#endif

// #if CONFIG_CPU == S5L8702
#if 0                  
    /* START */
    wait_rdy(bus);
    IICDS(bus) = slave | rd;
    wait_rdy(bus);
    IICSTAT(bus) = rd ? 0xB0 : 0xF0;
#else
    /* START */
    int mode = rd ? 0x80 : 0xC0;
    wait_rdy(bus);
    IICSTAT(bus) = mode;
    wait_rdy(bus);
    IICDS(bus) = slave | rd;
    wait_rdy(bus);
    IICSTAT(bus) = mode | 0x30;
#endif

#if CONFIG_CPU == S5L8702
    // XXX this seems needed when ECLK is used
    wait_rdy(bus);      // XXX: para solucionar problema del nano3g con ECLK, o se puede poner en cualquiera de las dos i2c_wait_io(), ambas funcionan si se pone esto
                        // TODO: probar classic, igual no hace falta
#endif
// caller=0;
    i2c_wait_io(bus);

    /* check ACK */
    if (IICSTAT(bus) & 1)
        return 1;

    return 0;
}

static void i2c_stop(int bus)
{
    /* STOP */
    wait_rdy(bus);
    IICSTAT(bus) &= ~0x20;
    wait_rdy(bus);
    IICCON(bus) = 0x10;
//IICCON(bus) = IICCON(bus);            // XXX: esto hace OF
// caller=1;

// wait_rdy(bus);
    i2c_wait_io(bus);                   // XXX: esto es para limpiar los flags de STA2 ???, no lo hace OF
}

static int i2c_wr_internal(int bus, unsigned char slave,
                    int address, int len, const unsigned char *data)
{
    int rc = 0;

    if (i2c_start(bus, slave, false) == 0)
    {
        /* write address + data */
        const unsigned char *ptr = data;
        const unsigned char addr = address;
        if (address >= 0) {
            ptr = &addr;
            len++;
        }
        while (len--) {
            wait_rdy(bus);
            IICDS(bus) = *ptr;
            udelay(5);
            wait_rdy(bus);
            IICCON(bus) = IICCON(bus);
// caller=2;
// wait_rdy(bus);
            i2c_wait_io(bus);
            /* check ACK */
            if (IICSTAT(bus) & 1) {
                rc = 2;
                break;
            }
            if (ptr == &addr) ptr = data;
            else ptr++;
        }
    }
    else
        rc = 1;

    i2c_stop(bus);
    return rc;
}

static int i2c_rd_internal(int bus, unsigned char slave, int len, unsigned char *data)
{
    int rc = 0;

    if (i2c_start(bus, slave, true) == 0)
    {
        while (len--) {
            wait_rdy(bus);
            IICCON(bus) &= ~(len ? 0 : 0x80); /* ACK or NAK */
// caller=3;
// wait_rdy(bus);
            i2c_wait_io(bus);
            *data++ = IICDS(bus);
        }
    }
    else
        rc = 3;

    i2c_stop(bus);
    return rc;
}

int i2c_wr(int bus, unsigned char slave, int address, int len, const unsigned char *data)
{
    i2c_on(bus);
    int rc = i2c_wr_internal(bus, slave, address, len, data);
    i2c_off(bus);
    return rc;
}

#if 1 // DEBUG
uint32_t i2c_elapsed = 1;
#endif

int i2c_rd(int bus, unsigned char slave, int address, int len, unsigned char *data)
{
    i2c_on(bus);
    int rc = i2c_wr_internal(bus, slave, address, 0, NULL);
#if 1 // DEBUG
if (rc == 0)
{
    i2c_elapsed = USEC_TIMER;
    rc = i2c_rd_internal(bus, slave, len, data);
    i2c_elapsed = USEC_TIMER - i2c_elapsed;
}
#else
    if (rc == 0)
        rc = i2c_rd_internal(bus, slave, len, data);
#endif
    i2c_off(bus);
    return rc;
}

int i2c_err[3];

int i2c_write(int bus, unsigned char slave, int address, int len, const unsigned char *data)
{
    int ret;
    mutex_lock(&i2c_mtx[bus]);
    ret = i2c_wr(bus, slave, address, len, data);
    mutex_unlock(&i2c_mtx[bus]);
if (ret != 0) i2c_err[ret-1]++;
    return ret;
}

int i2c_read(int bus, unsigned char slave, int address, int len, unsigned char *data)
{
    int ret;
    mutex_lock(&i2c_mtx[bus]);
    ret = i2c_rd(bus, slave, address, len, data);
    mutex_unlock(&i2c_mtx[bus]);
if (ret != 0) i2c_err[ret-1]++;
    return ret;
}

void i2c_preinit(int bus)
{
#if CONFIG_CPU == S5L8702
    if (bus == 0)
        PCON3 = (PCON3 & ~0x00000ff0) | 0x00000220;
    #if 0 /* TBC */
    else if (bus == 1)
        PCON6 = (PCON6 & ~0x0ff00000) | 0x02200000;
    #endif
#else /* S5L8720 */
    if (bus == 0) {
        PCON2 = (PCON2 & ~0xf0000000) | 0x20000000;
        PCON3 = (PCON3 & ~0x0000000f) | 0x00000002;
    }
#endif
    i2c_on(bus);
    wait_rdy(bus);
    IICADD(bus) = 0x40;   /* own slave address */
    wait_rdy(bus);
#if CONFIG_CPU == S5L8702
//     IICUNK14(bus) = 0;
IICUNK14(bus) = 1;    /* SRCCLK = ECLK */
#else /* S5L8720 */
// IICUNK14(bus) = 0;
    IICUNK14(bus) = 1;    /* SRCCLK = ECLK */
#endif
    wait_rdy(bus);
    IICUNK18(bus) = 0;
    wait_rdy(bus);
    IICSTAT(bus) = 0x80;  /* master Rx mode, serial output off */
    wait_rdy(bus);
    IICCON(bus) = 0;
    wait_rdy(bus);
    IICSTA2(bus) = 0x3f00;
    i2c_off(bus);
}
