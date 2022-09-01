/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
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

#include "wmcodec.h"
#include "audiohw.h"

/* Register addresses as per datasheet Rev.4.4 */
#define RESET       0x00
#define PWRMGMT1    0x01
#define PWRMGMT2    0x02
#define PWRMGMT3    0x03
#define AINTFCE     0x04
#define COMPAND     0x05
#define CLKGEN      0x06
#define SRATECTRL   0x07
#define GPIOCTL     0x08
#define JACKDETECT0 0x09
#define DACCTRL     0x0a
#define LDACVOL     0x0b
#define RDACVOL     0x0c
#define JACKDETECT1 0x0d
#define ADCCTL      0x0e
#define LADCVOL     0x0f
#define RADCVOL     0x10
#define RDACVOL_DACVU      0x100

#define EQ1         0x12
#define EQ2         0x13
#define EQ3         0x14
#define EQ4         0x15
#define EQ5         0x16
#define EQ_GAIN_MASK       0x001f
#define EQ_CUTOFF_MASK     0x0060
#define EQ_GAIN_VALUE(x)   (((-x) + 12) & 0x1f)
#define EQ_CUTOFF_VALUE(x) ((((x) - 1) & 0x03) << 5)

#define CLASSDCTL   0x17
#define DACLIMIT1   0x18
#define DACLIMIT2   0x19
#define NOTCH1      0x1b
#define NOTCH2      0x1c
#define NOTCH3      0x1d
#define NOTCH4      0x1e
#define ALCCTL1     0x20
#define ALCCTL2     0x21
#define ALCCTL3     0x22
#define NOISEGATE   0x23
#define PLLN        0x24
#define PLLK1       0x25
#define PLLK2       0x26
#define PLLK3       0x27
#define THREEDCTL   0x29
#define OUT4ADC     0x2a
#define BEEPCTRL    0x2b
#define INCTRL      0x2c
#define LINPGAGAIN  0x2d
#define RINPGAGAIN  0x2e
#define LADCBOOST   0x2f
#define RADCBOOST   0x30
#define OUTCTRL     0x31
#define LOUTMIX     0x32
#define ROUTMIX     0x33
#define LOUT1VOL    0x34
#define ROUT1VOL    0x35
#define LOUT2VOL    0x36
#define ROUT2VOL    0x37
#define OUT3MIX     0x38
#define OUT4MIX     0x39
#define BIASCTL     0x3d

/* shadow registers */
static unsigned int eq1_reg;
static unsigned int eq5_reg;

/* convert tenth of dB volume (-89..6) to master volume register value */
static int vol_tenthdb2hw(int db)
{
     /* Might have no sense, taken from wm8758.c :
         att  DAC  AMP  result
         +6dB    0   +6     96
          0dB    0    0     90
        -57dB    0  -57     33
        -58dB   -1  -57     32
        -89dB  -32  -57      1
        -90dB  -oo  -oo      0 */

    if (db <= -900) {
        return 0;
    } else {
        return db / 10 - -90;
    }
}

 /* helper function coming from wm8758.c that calculates the register setting for amplifier and
    DAC volume out of the input from tenthdb2master() */
static void get_volume_params(int db, int *dac, int *amp)
{
    /* should never happen, set max volume for amp and dac */
    if      (db > 96) {
        *dac = 255;
        *amp = 63;
    }
    /* set dac to max and set volume for amp (better snr) */
    else if (db > 32) {
        *dac = 255;
        *amp = (db-90)+57;
    }
    /* set amp to min and reduce dac output */
    else if (db >  0) {
        *dac = (db-33)*2 + 255;
        *amp = 0;
    }
    /* mute all */
    else {
        *dac = 0x00;
        *amp = 0x40;
    }
}

/* Silently enable / disable audio output */
void audiohw_preinit(void)
{
    wmcodec_write(RESET,    0x1ff);    /* Reset */

    wmcodec_write(BIASCTL,  0x100); /* BIASCUT = 1 */
    wmcodec_write(OUTCTRL,  0x6);   /* Thermal shutdown */

    wmcodec_write(PWRMGMT1, 0x8);   /* BIASEN = 1 */

    /* Volume zero, mute all outputs */
    wmcodec_write(LOUT1VOL, 0x140);
    wmcodec_write(ROUT1VOL, 0x140);
    wmcodec_write(LOUT2VOL, 0x140);
    wmcodec_write(ROUT2VOL, 0x140);
    wmcodec_write(OUT3MIX,  0x40);
    wmcodec_write(OUT4MIX,  0x40);

    /* DAC softmute, automute, 128OSR */
    wmcodec_write(DACCTRL,  0x4c);

    wmcodec_write(OUT4ADC,  0x2);   /* POBCTRL = 1 */

    /* Enable output, DAC and mixer */
    wmcodec_write(PWRMGMT3, 0x6f);
    wmcodec_write(PWRMGMT2, 0x180);
    wmcodec_write(PWRMGMT1, 0xd);
    wmcodec_write(LOUTMIX,  0x1);
    wmcodec_write(ROUTMIX,  0x1);

    /* Disable clock since we're acting as slave to the SoC */
    wmcodec_write(CLKGEN,  0x0);
    wmcodec_write(AINTFCE, 0x10);   /* 16-bit, I2S format */

    wmcodec_write(LDACVOL, 0x1ff);  /* Full DAC digital vol */
    wmcodec_write(RDACVOL, 0x1ff);

    wmcodec_write(OUT4ADC, 0x0);    /* POBCTRL = 0 */
}

static void audiohw_mute(bool mute)
{
    if (mute)
    {
        /* Set DACMU = 1 to soft-mute the audio DACs. */
        wmcodec_write(DACCTRL, 0x4c);
    } else {
        /* Set DACMU = 0 to soft-un-mute the audio DACs. */
        wmcodec_write(DACCTRL, 0xc);
    }
}

void audiohw_postinit(void)
{
    sleep(HZ/2);

    audiohw_mute(0);
}

void audiohw_set_volume(int vol_l, int vol_r)
{
    int dac_l, amp_l, dac_r, amp_r;

    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    get_volume_params(vol_l, &dac_l, &amp_l);
    get_volume_params(vol_r, &dac_r, &amp_r);
 
    /* set DAC
       Important: DAC is global and will also affect lineout */
    wmcodec_write(LDACVOL, dac_l);
    wmcodec_write(RDACVOL, dac_r | RDACVOL_DACVU);

    /* set headphone amp OUT1 */
    wmcodec_write(LOUT1VOL, amp_l | 0x080);
    wmcodec_write(ROUT1VOL, amp_r | 0x180);
}

void audiohw_set_lineout_volume(int vol_l, int vol_r)
{
    int dac_l, amp_l, dac_r, amp_r;

    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    get_volume_params(vol_l, &dac_l, &amp_l);
    get_volume_params(vol_r, &dac_r, &amp_r);
 
    /* set lineout amp OUT2 */
    wmcodec_write(LOUT2VOL, amp_l);
    wmcodec_write(ROUT2VOL, amp_r | 0x100);
}

void audiohw_set_aux_vol(int vol_l, int vol_r)
{
    /* OUTMIX */
    wmcodec_write(LOUTMIX, 0x111 | (vol_l << 5) );
    wmcodec_write(ROUTMIX, 0x111 | (vol_r << 5) );
}

#ifdef AUDIOHW_HAVE_BASS
void audiohw_set_bass(int value)
{
    eq1_reg = (eq1_reg & ~EQ_GAIN_MASK) | EQ_GAIN_VALUE(value);
    wmcodec_write(EQ1, 0x100 | eq1_reg);
}
#endif /* AUDIOHW_HAVE_BASS */

#ifdef AUDIOHW_HAVE_BASS_CUTOFF
void audiohw_set_bass_cutoff(int value)
{
    eq1_reg = (eq1_reg & ~EQ_CUTOFF_MASK) | EQ_CUTOFF_VALUE(value);
    wmcodec_write(EQ1, 0x100 | eq1_reg);
}
#endif /* AUDIOHW_HAVE_BASS_CUTOFF */

#ifdef AUDIOHW_HAVE_TREBLE
void audiohw_set_treble(int value)
{
    eq5_reg = (eq5_reg & ~EQ_GAIN_MASK) | EQ_GAIN_VALUE(value);
    wmcodec_write(EQ5, eq5_reg);
}
#endif /* AUDIOHW_HAVE_TREBLE */

#ifdef AUDIOHW_HAVE_TREBLE_CUTOFF
void audiohw_set_treble_cutoff(int value)
{
    eq5_reg = (eq5_reg & ~EQ_CUTOFF_MASK) | EQ_CUTOFF_VALUE(value);
    wmcodec_write(EQ5, eq5_reg);
}
#endif /* AUDIOHW_HAVE_TREBLE_CUTOFF */

/* Nice shutdown of WM8985 codec */
void audiohw_close(void)
{
    audiohw_mute(1);

    wmcodec_write(PWRMGMT3, 0x0);

    wmcodec_write(PWRMGMT1, 0x0);

    wmcodec_write(PWRMGMT2, 0x40);
}

#if 0 /* function is currently unused */
/* Note: Disable output before calling this function */
void audiohw_set_sample_rate(int fsel)
{
    /* Currently the WM8985 acts as slave to the SoC I2S controller, so no
       setup is needed here. This seems to be in contrast to every other WM
       driver in Rockbox, so this may need to change in the future. */
    (void)fsel;
}
#endif

#ifdef HAVE_RECORDING
void audiohw_enable_recording(bool source_mic)
{
    (void)source_mic; /* We only have a line-in (I think) */

    wmcodec_write(RESET, 0x1ff);    /*Reset*/

    wmcodec_write(PWRMGMT1, 0x2b);
    wmcodec_write(PWRMGMT2, 0x18f);  /* Enable ADC - 0x0c enables left/right PGA input, and 0x03 turns on power to the ADCs */
    wmcodec_write(PWRMGMT3, 0x6f);

    wmcodec_write(AINTFCE, 0x10);
    wmcodec_write(CLKCTRL, 0x49);

    wmcodec_write(OUTCTRL, 1);

    /* The iPod can handle multiple frequencies, but fix at 44.1KHz
       for now */
    audiohw_set_frequency(HW_FREQ_DEFAULT);

    wmcodec_write(INCTRL,0x44);  /* Connect L2 and R2 inputs */

    /* Set L2/R2_2BOOSTVOL to 0db (bits 4-6) */
    /* 000 = disabled
       001 = -12dB
       010 = -9dB
       011 = -6dB
       100 = -3dB
       101 = 0dB
       110 = 3dB
       111 = 6dB
    */
    wmcodec_write(LADCBOOST,0x50);
    wmcodec_write(RADCBOOST,0x50);

    /* Set L/R input PGA Volume to 0db */
    //    wm8758_write(LINPGAVOL,0x3f);
    //    wm8758_write(RINPGAVOL,0x13f);

    /* Enable monitoring */
    wmcodec_write(LOUTMIX,0x17); /* Enable output mixer - BYPL2LMIX @ 0db*/
    wmcodec_write(ROUTMIX,0x17); /* Enable output mixer - BYPR2RMIX @ 0db*/

    audiohw_mute(0);
}

void audiohw_disable_recording(void) {
    audiohw_mute(1);

    wmcodec_write(PWRMGMT3, 0x0);

    wmcodec_write(PWRMGMT1, 0x0);

    wmcodec_write(PWRMGMT2, 0x40);
}

void audiohw_set_recvol(int left, int right, int type) {

    (void)left;
    (void)right;
    (void)type;
}

void audiohw_set_monitor(bool enable) {

    (void)enable;
}
#endif /* HAVE_RECORDING */
