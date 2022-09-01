/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Driver for WM8975 audio codec
 *
 * Based on code from the ipodlinux project - http://ipodlinux.org/
 * Adapted for Rockbox in December 2005
 *
 * Original file: linux/arch/armnommu/mach-ipod/audio.c
 *
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
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

#include "logf.h"
#include "system.h"
#include "kernel.h"
#include "string.h"
#include "audio.h"
#include "sound.h"

#include "wmcodec.h"
#include "audiohw.h"

static unsigned short wm8975_regs[WM8975_NUM_REGISTERS] =
{
    [LINVOL]   = LINVOL_LZCEN | 23, /* 0dB */
    [RINVOL]   = RINVOL_RIVU | RINVOL_RZCEN | 23, /* 0dB */
    [DAPCTRL]  = DAPCTRL_DACMU,
/* This reduces the popping noise during codec powerup
   noticably, especially with high-impedance loads.
   We might want to change this for all targets,
   but it has only been tested on iPod Nano 2G so far. */
#ifdef IPOD_NANO2G
    [PWRMGMT1] = PWRMGMT1_VMIDSEL_500K | PWRMGMT1_VREF,
#else
    [PWRMGMT1] = PWRMGMT1_VMIDSEL_5K | PWRMGMT1_VREF,
#endif
    [PWRMGMT2] = PWRMGMT2_DACL | PWRMGMT2_DACR | PWRMGMT2_LOUT1 
                 | PWRMGMT2_ROUT1 | PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2,
};

static void wm8975_write(int reg, unsigned val)
{
    if (WM8975_NUM_REGISTERS > reg) {
        wm8975_regs[reg] = val;
        wmcodec_write(reg, val);
    }
}

static void wm8975_write_and(int reg, unsigned bits)
{
    wm8975_write(reg, wm8975_regs[reg] & bits);
}

static void wm8975_write_or(int reg, unsigned bits)
{
    wm8975_write(reg, wm8975_regs[reg] | bits);
}

/* convert tenth of dB volume (-730..60) to master volume register value */
static int vol_tenthdb2hw(int db)
{
    /* +6 to -73dB 1dB steps (plus mute == 80levels) 7bits */
    /* 1111111 == +6dB  (0x7f) */
    /* 1111001 == 0dB   (0x79) */
    /* 0110000 == -73dB (0x30 */
    /* 0101111..0000000 == mute  (0x2f) */

    if (db <= -740) {
        return 0x0;
    } else {
        return((db/10)+73+0x30);
    }
}

static void audiohw_mute(bool mute)
{
    if (mute) {
        /* Set DACMU = 1 to soft-mute the audio DACs. */
        wm8975_write_or(DAPCTRL, DAPCTRL_DACMU);
    } else {
        /* Set DACMU = 0 to soft-un-mute the audio DACs. */
        wm8975_write_and(DAPCTRL, ~DAPCTRL_DACMU);
    }
}

#define IPOD_PCM_LEVEL 0x65       /* -6dB */

/* This reduces the popping noise during codec powerup
   noticably, especially with high-impedance loads.
   We might want to change this for all targets,
   but it has only been tested on iPod Nano 2G so far. */
#ifdef IPOD_NANO2G
void audiohw_preinit(void)
{
    wm8975_write(RESET, RESET_RESET);

    wm8975_write(AINTFCE, AINTFCE_MS | AINTFCE_LRP_I2S_RLO
                        | AINTFCE_IWL_16BIT | AINTFCE_FORMAT_I2S);

    wm8975_write(LOUTMIX1, LOUTMIX1_LD2LO | LOUTMIX1_LI2LOVOL(5));
    wm8975_write(ROUTMIX2, ROUTMIX2_RD2RO | ROUTMIX2_RI2ROVOL(5));

    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1]);
    wm8975_write(PWRMGMT2, wm8975_regs[PWRMGMT2]);
}

void audiohw_postinit(void)
{
    wm8975_regs[PWRMGMT1] &= ~PWRMGMT1_VMIDSEL_MASK;
    wm8975_regs[PWRMGMT1] |= PWRMGMT1_VMIDSEL_50K;
    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1]);
    audiohw_mute(false);
}
#else /* !IPOD_NANO2G */
void audiohw_preinit(void)
{
    /* POWER UP SEQUENCE */
    wm8975_write(RESET, RESET_RESET);

    /* 2. Enable Vmid and VREF, quick startup. */
    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1]);
    sleep(HZ/50);
    wm8975_regs[PWRMGMT1] &= ~PWRMGMT1_VMIDSEL_MASK;
    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1] | PWRMGMT1_VMIDSEL_50K);
    
    /* 4. Enable DACs, line and headphone output buffers as required. */
    wm8975_write(PWRMGMT2, wm8975_regs[PWRMGMT2]);
    
    wm8975_write(AINTFCE, AINTFCE_MS | AINTFCE_LRP_I2S_RLO
                        | AINTFCE_IWL_16BIT | AINTFCE_FORMAT_I2S);

    wm8975_write(DAPCTRL, wm8975_regs[DAPCTRL] );

    /* Set sample rate. */
    wm8975_write(SAMPCTRL, WM8975_44100HZ);

    /* set the volume to -6dB */
    wm8975_write(LOUT1VOL, LOUT1VOL_LO1ZC | IPOD_PCM_LEVEL);
    wm8975_write(ROUT1VOL, ROUT1VOL_RO1VU | ROUT1VOL_RO1ZC | IPOD_PCM_LEVEL);

    wm8975_write(LOUTMIX1, LOUTMIX1_LD2LO| LOUTMIX1_LI2LOVOL(5));
    wm8975_write(LOUTMIX2, LOUTMIX2_RI2LOVOL(5));
    
    wm8975_write(ROUTMIX1, ROUTMIX1_LI2ROVOL(5));
    wm8975_write(ROUTMIX2, ROUTMIX2_RD2RO| ROUTMIX2_RI2ROVOL(5));
    
    wm8975_write(MOUTMIX1, 0);
    wm8975_write(MOUTMIX2, 0);
}

void audiohw_postinit(void)
{
    audiohw_mute(false);
}
#endif

void audiohw_set_volume(int vol_l, int vol_r)
{
    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    /* OUT1 */
    wm8975_write(LOUT1VOL, LOUT1VOL_LO1ZC | vol_l);
    wm8975_write(ROUT1VOL, ROUT1VOL_RO1VU | ROUT1VOL_RO1ZC | vol_r);
}

void audiohw_set_lineout_volume(int vol_l, int vol_r)
{
    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    /* OUT2 */
    wm8975_write(LOUT2VOL, LOUT2VOL_LO2ZC | vol_l);
    wm8975_write(ROUT2VOL, ROUT2VOL_RO2VU | ROUT2VOL_RO2ZC | vol_r);
}

void audiohw_enable_lineout(bool enable)
{
    if (enable) {
        /* Enable lineout */
        wm8975_regs[PWRMGMT2] |=  (PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2);
    } else {
        /* Disable lineout */
        wm8975_regs[PWRMGMT2] &= ~(PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2);
    }
    wm8975_write(PWRMGMT2, wm8975_regs[PWRMGMT2]);
}

void audiohw_set_bass(int value)
{
    const int regvalues[] = {
        11, 10, 10, 9, 8, 8, 0xf, 6, 6, 5, 4, 4, 3, 2, 2, 1
    };

    if ((value >= -6) && (value <= 9)) {
        /* We use linear bass control with 200 Hz cutoff */
        wm8975_write(BASSCTRL, regvalues[value + 6] | BASSCTRL_BC);
    }
}

void audiohw_set_treble(int value)
{
    const int regvalues[] = {
        11, 10, 10, 9, 8, 8, 0xf, 6, 6, 5, 4, 4, 3, 2, 2, 1
    };

    if ((value >= -6) && (value <= 9)) {
        /* We use linear treble control with 4 kHz cutoff */
        wm8975_write(TREBCTRL, regvalues[value + 6] | TREBCTRL_TC);
    }
}

/* Nice shutdown of WM8975 codec */
void audiohw_close(void)
{
    audiohw_mute(true);

    /* 2. Disable all output buffers. */
    wm8975_write(PWRMGMT2, 0x0);

    /* 3. Switch off the power supplies. */
    wm8975_write(PWRMGMT1, 0x0);
}

/* Note: Disable output before calling this function */
void audiohw_set_frequency(int fsel)
{
    (void)fsel;
}

#ifdef HAVE_RECORDING
void audiohw_enable_recording(bool source_mic)
{
    wm8975_regs[PWRMGMT1] |= PWRMGMT1_AINL | PWRMGMT1_AINR
                           | PWRMGMT1_ADCL | PWRMGMT1_ADCR;
    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1]);

    /* NOTE: When switching to digital monitoring we will not want
     * the DACs disabled. Also the outputs shouldn't be disabled
     * when recording from line in (dock connector) - needs testing. */
    wm8975_regs[PWRMGMT2] &= ~(PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1
                             | PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2);
    wm8975_write(PWRMGMT2, wm8975_regs[PWRMGMT2]);

    wm8975_write_or(LINVOL, LINVOL_LINMUTE);
    wm8975_write_or(RINVOL, RINVOL_RINMUTE);

    wm8975_write(ADDCTRL3, ADDCTRL3_VROI);

    if (source_mic) {
        wm8975_write(ADDCTRL1, ADDCTRL1_VSEL_LOWBIAS | ADDCTRL1_DATSEL_RADC
                             | ADDCTRL1_TOEN);
        wm8975_write(ADCLPATH, 0);
        wm8975_write(ADCRPATH, ADCRPATH_RINSEL_RIN2 | ADCRPATH_RMICBOOST_20dB);
    } else {
        wm8975_write(ADDCTRL1, ADDCTRL1_VSEL_LOWBIAS | ADDCTRL1_DATSEL_NORMAL
                             | ADDCTRL1_TOEN);
        wm8975_write(ADCLPATH, ADCLPATH_LINSEL_LIN1 | ADCLPATH_LMICBOOST_OFF);
        wm8975_write(ADCRPATH, ADCRPATH_RINSEL_RIN1 | ADCRPATH_RMICBOOST_OFF);
    }
    wm8975_write_and(LINVOL, ~LINVOL_LINMUTE);
    wm8975_write_and(RINVOL, ~RINVOL_RINMUTE);
}

void audiohw_disable_recording(void) 
{
    /* mute inputs */
    wm8975_write_or(LINVOL, LINVOL_LINMUTE);
    wm8975_write_or(RINVOL, RINVOL_RINMUTE);

    wm8975_write(ADDCTRL3, 0);

    wm8975_regs[PWRMGMT2] |= PWRMGMT2_DACL | PWRMGMT2_DACR
                           | PWRMGMT2_LOUT1 | PWRMGMT2_ROUT1
                           | PWRMGMT2_LOUT2 | PWRMGMT2_ROUT2;
    wm8975_write(PWRMGMT2, wm8975_regs[PWRMGMT2]);

    wm8975_regs[PWRMGMT1] &= ~(PWRMGMT1_AINL | PWRMGMT1_AINR
                               | PWRMGMT1_ADCL | PWRMGMT1_ADCR);
    wm8975_write(PWRMGMT1, wm8975_regs[PWRMGMT1]);
}

/* volume in 0 .. 63, corresponds to -17.25dB .. 30dB in steps of 0.75dB
 * microphone has an extra 20dB boost so 0 .. 63 corresponds to 2.75dB .. 50dB */
void audiohw_set_recvol(int left, int right, int type)
{
    switch (type)
    {
    case AUDIO_GAIN_MIC:  /* Mic uses right ADC */
        wm8975_regs[RINVOL] &= ~RINVOL_MASK;
        wm8975_write_or(RINVOL, left & RINVOL_MASK);
        break;
    case AUDIO_GAIN_LINEIN:
        wm8975_regs[LINVOL] &= ~LINVOL_MASK;
        wm8975_write_or(LINVOL, left & LINVOL_MASK);
        wm8975_regs[RINVOL] &= ~RINVOL_MASK;
        wm8975_write_or(RINVOL, right & RINVOL_MASK);
        break;
    default:
        return;
    }
}

void audiohw_set_monitor(bool enable)
{
    if (enable) {
        /* set volume to 0 dB */
        wm8975_regs[LOUTMIX1] &= ~LOUTMIX1_LI2LOVOL_MASK;
        wm8975_regs[LOUTMIX1] |=  LOUTMIX1_LI2LOVOL(2);
        wm8975_regs[ROUTMIX2] &= ~ROUTMIX2_RI2ROVOL_MASK;
        wm8975_regs[ROUTMIX2] |=  ROUTMIX2_RI2ROVOL(2);
        /* set mux to line input */
        wm8975_write_and(LOUTMIX1, ~LOUTMIX1_LMIXSEL_MASK);
        wm8975_write_and(ROUTMIX1, ~ROUTMIX1_RMIXSEL_MASK);
        /* enable bypass */
        wm8975_write_or(LOUTMIX1, LOUTMIX1_LI2LO);
        wm8975_write_or(ROUTMIX2, ROUTMIX2_RI2RO);
    } else {
        /* disable bypass */
        wm8975_write_and(LOUTMIX1, ~LOUTMIX1_LI2LO);
        wm8975_write_and(ROUTMIX2, ~ROUTMIX2_RI2RO);
    }
}
#endif /* HAVE_RECORDING */
