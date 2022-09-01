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
#include "system.h"
#include "kernel.h"
#include "thread.h"
#include "string.h"
#include "adc.h"
#include "pcf50605.h"
#include "i2c-pp.h"

struct adc_struct {
    long timeout;
    void (*conversion)(unsigned short *data);
    short channelnum;
    unsigned short data;
};

static struct adc_struct adcdata[NUM_ADC_CHANNELS] IDATA_ATTR;

static unsigned short _adc_read(struct adc_struct *adc)
{
    if (TIME_AFTER(current_tick, adc->timeout)) {
        unsigned char data[2];
        unsigned short value;

        i2c_lock();

        /* 5x per 2 seconds */
        adc->timeout = current_tick + (HZ * 2 / 5);

        /* ADCC1, 10 bit, start */
        pcf50605_write(0x2f, (adc->channelnum << 1) | 0x1);
        pcf50605_read_multiple(0x30, data, 2); /* ADCS1, ADCS2 */
        value   = data[0];
        value <<= 2;
        value  |= data[1] & 0x3;

        if (adc->conversion) {
            adc->conversion(&value);
        }
        adc->data = value;

        i2c_unlock();
        return value;
    } else
    {
        return adc->data;
    }
}

/* Force an ADC scan _now_ */
unsigned short adc_scan(int channel) {
    struct adc_struct *adc = &adcdata[channel];
    adc->timeout = 0;
    return _adc_read(adc);
}

/* Retrieve the ADC value, only does a scan periodically */
unsigned short adc_read(int channel) {
    return _adc_read(&adcdata[channel]);
}

void adc_init(void)
{
    struct adc_struct *adc_battery = &adcdata[ADC_BATTERY];
    adc_battery->channelnum = 0x2; /* ADCVIN1, resistive divider */
    adc_battery->timeout = 0;
    adcdata[ADC_ACCESSORY].channelnum = 4;
    adcdata[ADC_ACCESSORY].timeout = 0;
#if defined(IPOD_VIDEO) || defined(IPOD_NANO)
    adcdata[ADC_4066_ISTAT].channelnum = 7;
    adcdata[ADC_4066_ISTAT].timeout = 0;
#endif
    _adc_read(adc_battery);
}
