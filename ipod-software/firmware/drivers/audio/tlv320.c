/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Christian Gmeiner
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
#include "logf.h"
#include "string.h"
#include "audio.h"

#if CONFIG_I2C == I2C_COLDFIRE
#include "i2c-coldfire.h"
#elif CONFIG_I2C == I2C_DM320
#include "i2c-dm320.h"
#endif
#include "audiohw.h"

/* convert tenth of dB volume (-73..6) to master volume register value */
static int vol_tenthdb2hw(int db)
{
    /* +6 to -73dB 1dB steps (plus mute == 80levels) 7bits */
    /* 1111111 == +6dB  (0x7f) */
    /* 1111001 == 0dB   (0x79) */
    /* 0110000 == -73dB (0x30) */
    /* 0101111 == mute  (0x2f) */

    if (db <= -740) {
        return 0x2f;
    } else {
        return((db/10)+73+0x30);
    }
}

/* local functions and definations */
#ifndef CREATIVE_ZVx
#define TLV320_ADDR 0x34
#else
#define TLV320_ADDR 0x1A
#endif

static struct tlv320_info
{
    int vol_l;
    int vol_r;
} tlv320;

/* Shadow registers */
static unsigned tlv320_regs[0xf];

static void tlv320_write_reg(unsigned reg, unsigned value)
{
    unsigned char data[2];

    /* The register address is the high 7 bits and the data the low 9 bits */
    data[0] = (reg << 1) | ((value >> 8) & 1);
    data[1] = value;

#if CONFIG_I2C == I2C_COLDFIRE
    if (i2c_write(I2C_IFACE_0, TLV320_ADDR, data, 2) != 2)
#elif CONFIG_I2C == I2C_DM320
    if (i2c_write(TLV320_ADDR, data, 2) != 0)
#else
    #warning Implement tlv320_write_reg()
#endif
    {
        logf("tlv320 error reg=0x%x", reg);
        return;
    }

    tlv320_regs[reg] = value;
}

static void audiohw_mute(bool mute)
{
    unsigned value_dap = tlv320_regs[REG_DAP];
    unsigned value_l, value_r;

    if (mute)
    {
        value_l = LHV_LHV(HEADPHONE_MUTE);
        value_r = RHV_RHV(HEADPHONE_MUTE);
        value_dap |= DAP_DACM;
    }
    else
    {
        value_l = LHV_LHV(tlv320.vol_l);
        value_r = RHV_RHV(tlv320.vol_r);
        if (value_l > HEADPHONE_MUTE || value_r > HEADPHONE_MUTE)
            value_dap &= ~DAP_DACM;
    }

    tlv320_write_reg(REG_LHV, LHV_LZC | value_l);
    tlv320_write_reg(REG_RHV, RHV_RZC | value_r);
    tlv320_write_reg(REG_DAP, value_dap);
}

/* public functions */

/**
 * Init our tlv with default values
 */
void audiohw_init(void)
{
    logf("TLV320 init");
    memset(tlv320_regs, 0, sizeof(tlv320_regs));

    /* Initialize all registers */

    /* All ON except OUT, ADC, MIC and LINE */
    tlv320_write_reg(REG_PC, PC_OUT | PC_ADC | PC_MIC | PC_LINE);
#ifdef HAVE_RECORDING
    audiohw_set_recvol(0, 0, AUDIO_GAIN_MIC);
    audiohw_set_recvol(0, 0, AUDIO_GAIN_LINEIN);
#endif
    audiohw_mute(true);
    tlv320_write_reg(REG_AAP, AAP_DAC | AAP_MICM);
    tlv320_write_reg(REG_DAP, 0x00);  /* No deemphasis */
#ifndef CREATIVE_ZVx
    tlv320_write_reg(REG_DAIF, DAIF_IWL_16 | DAIF_FOR_I2S);
#else
    tlv320_write_reg(REG_DAIF, DAIF_IWL_32 | DAIF_FOR_DSP);
#endif
    tlv320_write_reg(REG_DIA, DIA_ACT);
    audiohw_set_frequency(-1); /* default */
}

/**
 * Switch outputs ON
 */
void audiohw_postinit(void)
{
    /* All ON except ADC, MIC and LINE */
    sleep(HZ);
    tlv320_write_reg(REG_PC, PC_ADC | PC_MIC | PC_LINE);
    sleep(HZ/4);
    audiohw_mute(false);
}

/**
 * Sets internal sample rate for DAC and ADC relative to MCLK
 * Selection for frequency:
 * Fs:        tlv:   with:
 * 11025: 0 = MCLK/2 MCLK/2  SCLK, LRCK: Audio Clk / 16
 * 22050: 0 = MCLK/2 MCLK    SCLK, LRCK: Audio Clk / 8
 * 44100: 1 = MCLK   MCLK    SCLK, LRCK: Audio Clk / 4 (default)
 * 88200: 2 = MCLK*2 MCLK    SCLK, LRCK: Audio Clk / 2
 */
void audiohw_set_frequency(int fsel)
{
    /* All rates available for 11.2896MHz besides 8.021 */
    static const unsigned char values_src[HW_NUM_FREQ] =
    {
        HW_HAVE_11_([HW_FREQ_11] = (0x8 << 2) | SRC_CLKIN,)
        HW_HAVE_22_([HW_FREQ_22] = (0x8 << 2) | SRC_CLKIN,)
        HW_HAVE_44_([HW_FREQ_44] = (0x8 << 2),)
        HW_HAVE_88_([HW_FREQ_88] = (0xf << 2),)
    };

    unsigned value_dap, value_pc;

    if ((unsigned)fsel >= HW_NUM_FREQ)
        fsel = HW_FREQ_DEFAULT;

    /* Temporarily turn off the DAC and ADC before switching sample
       rates or they don't choose their filters correctly */
    value_dap = tlv320_regs[REG_DAP];
    value_pc = tlv320_regs[REG_PC];

    tlv320_write_reg(REG_DAP, value_dap | DAP_DACM);
    tlv320_write_reg(REG_PC,  value_pc | PC_DAC | PC_ADC);
    tlv320_write_reg(REG_SRC, values_src[fsel]);
    tlv320_write_reg(REG_PC,  value_pc | PC_DAC);
    tlv320_write_reg(REG_PC,  value_pc);
    tlv320_write_reg(REG_DAP, value_dap);
}

/**
 * Sets left and right headphone volume
 *
 * Left & Right: 48 .. 121 .. 127 => Volume -73dB (mute) .. +0 dB .. +6 dB
 */
void audiohw_set_volume(int vol_l, int vol_r)
{
    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    unsigned value_dap = tlv320_regs[REG_DAP];
    unsigned value_dap_last = value_dap;
    unsigned value_l = LHV_LHV(vol_l);
    unsigned value_r = RHV_RHV(vol_r);

    /* keep track of current setting */
    tlv320.vol_l = vol_l;
    tlv320.vol_r = vol_r;

    if (value_l > HEADPHONE_MUTE || value_r > HEADPHONE_MUTE)
        value_dap &= ~DAP_DACM;
    else
        value_dap |= DAP_DACM;

    /* update */
    tlv320_write_reg(REG_LHV, LHV_LZC | value_l);
    tlv320_write_reg(REG_RHV, RHV_RZC | value_r);
    if (value_dap != value_dap_last)
        tlv320_write_reg(REG_DAP, value_dap);
}

/**
 * Set recording volume
 *
 * Line in   : 0 .. 31 => Volume -34.5 .. +12 dB
 * Mic (left): 0 ..  1 => Volume  +0,     +20 dB
 *
 */
#ifdef HAVE_RECORDING
void audiohw_set_recvol(int left, int right, int type)
{
    if (type == AUDIO_GAIN_MIC)
    {
        unsigned value_aap = tlv320_regs[REG_AAP];

        if (left)
            value_aap |= AAP_MICB;  /* Enable mic boost (20dB) */
        else
            value_aap &= ~AAP_MICB;

        tlv320_write_reg(REG_AAP, value_aap);
    }
    else if (type == AUDIO_GAIN_LINEIN)
    {
        tlv320_write_reg(REG_LLIV, LLIV_LIV(left));
        tlv320_write_reg(REG_RLIV, RLIV_RIV(right));
    }
}
#endif

/* Nice shutdown of TLV320 codec */
void audiohw_close(void)
{
    audiohw_mute(true);
    sleep(HZ/8);

    tlv320_write_reg(REG_PC, PC_OFF | PC_CLK | PC_OSC | PC_OUT |
        PC_DAC | PC_ADC | PC_MIC | PC_LINE);  /* All OFF */
}

#ifdef HAVE_RECORDING
void audiohw_enable_recording(bool source_mic)
{
    unsigned value_aap, value_pc;

    if (source_mic)
    {
        /* select MIC and enable mic boost (20 dB) */
        value_aap = AAP_DAC | AAP_INSEL | AAP_MICB;
        value_pc = PC_LINE;                 /* power down LINE */
    }
    else
    {
        value_aap = AAP_DAC | AAP_MICM;
        value_pc = PC_MIC;                  /* power down MIC */
    }

    tlv320_write_reg(REG_PC, value_pc);
    tlv320_write_reg(REG_AAP, value_aap);
}

void audiohw_disable_recording(void)
{
    unsigned value_pc = tlv320_regs[REG_PC];
    unsigned value_aap = tlv320_regs[REG_AAP];

    value_aap |= AAP_MICM;                 /* mute MIC */
    tlv320_write_reg(REG_PC, value_aap);

    value_pc |= PC_ADC | PC_MIC | PC_LINE; /* ADC, MIC and LINE off */
    tlv320_write_reg(REG_PC, value_pc);
}

void audiohw_set_monitor(bool enable)
{
    unsigned value_aap, value_pc;

    if (enable)
    {
        /* Keep DAC on to allow mixing of voice with analog audio */
        value_aap = AAP_DAC | AAP_BYPASS | AAP_MICM;
        value_pc  = PC_ADC | PC_MIC; /* ADC and MIC off */
    }
    else
    {
        value_aap = AAP_DAC | AAP_MICM;
        value_pc  = PC_ADC | PC_MIC | PC_LINE; /* ADC, MIC and LINE off */
    }

    tlv320_write_reg(REG_AAP, value_aap);
    tlv320_write_reg(REG_PC,  value_pc);
}
#endif /* HAVE_RECORDING */
