/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Driver for AS3514 and compatible audio codec
 *
 * Copyright (c) 2007 Daniel Ankers
 * Copyright (c) 2007 Christian Gmeiner
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

#include "cpu.h"
#include "kernel.h"
#include "debug.h"
#include "system.h"
#include "kernel.h"
#include "audio.h"
#include "sound.h"

#include "audiohw.h"
#include "i2s.h"
#include "ascodec.h"

#ifdef HAVE_AS3543
/* Headphone volume goes from -81.0 ... +6dB */
#define VOLUME_MIN -820
#define VOLUME_MAX   60
#else
/* Headphone volume goes from -73.5 ... +6dB */
#define VOLUME_MIN -740
#define VOLUME_MAX   60
#endif

/*
 * This drivers supports:
 * as3514 , as used in the PP targets
 * as3515 , as used in the as3525 targets
 * as3543 , as used in the as3525v2 and other as3543 targets
 */

#if CONFIG_CPU == AS3525
/* AMS Sansas based on the AS3525 use the LINE2 input for the analog radio
   signal instead of LINE1 */
#define AS3514_LINE_IN_R AS3514_LINE_IN2_R
#define AS3514_LINE_IN_L AS3514_LINE_IN2_L
#define ADC_R_ADCMUX_LINE_IN ADC_R_ADCMUX_LINE_IN2
#define AUDIOSET1_LIN_on AUDIOSET1_LIN2_on

#elif CONFIG_CPU == AS3525v2
/* There is only 1 pair of registers on AS3543, the line input is selectable in
   LINE_IN_R register */
#define AS3514_LINE_IN_R AS3514_LINE_IN1_R
#define AS3514_LINE_IN_L AS3514_LINE_IN1_L
#define ADC_R_ADCMUX_LINE_IN ADC_R_ADCMUX_LINE_IN2
#define AUDIOSET1_LIN_on AUDIOSET1_LIN1_on

#else   /* PP use line1 */

#define AS3514_LINE_IN_R AS3514_LINE_IN1_R
#define AS3514_LINE_IN_L AS3514_LINE_IN1_L
#define ADC_R_ADCMUX_LINE_IN ADC_R_ADCMUX_LINE_IN1
#define AUDIOSET1_LIN_on AUDIOSET1_LIN1_on

#endif

/* Shadow registers */
static uint8_t as3514_regs[AS3514_NUM_AUDIO_REGS]; /* 8-bit registers */
/* Keep track of volume */
static int current_vol_l, current_vol_r;

/*
 * little helper method to set register values.
 * With the help of as3514_regs, we minimize i2c/syscall
 * traffic.
 */
static void as3514_write(unsigned int reg, unsigned int value)
{
    ascodec_write(reg, value);

    if (reg < AS3514_NUM_AUDIO_REGS)
        as3514_regs[reg] = value;
}

/* Helpers to set/clear bits */
static void as3514_set(unsigned int reg, unsigned int bits)
{
    as3514_write(reg, as3514_regs[reg] | bits);
}

static void as3514_clear(unsigned int reg, unsigned int bits)
{
    as3514_write(reg, as3514_regs[reg] & ~bits);
}

static void as3514_write_masked(unsigned int reg, unsigned int bits,
                                unsigned int mask)
{
    as3514_write(reg, (as3514_regs[reg] & ~mask) | (bits & mask));
}

/* convert tenth of dB volume to master volume register value */
static int vol_tenthdb2hw(int db)
{
    if (db <= VOLUME_MIN) {
        return 0x0;
    } else if (db > VOLUME_MAX) {
        return (VOLUME_MAX - VOLUME_MIN) / 15;
    } else {
        return  (db - VOLUME_MIN) / 15;
    }
}

/*
 * Initialise the PP I2C and I2S.
 */
void audiohw_preinit(void)
{
    /* read all reg values */
    ascodec_readbytes(0x0, AS3514_NUM_AUDIO_REGS, as3514_regs);

#ifdef HAVE_AS3543
    /* Prevent increasing noise and power consumption if booted through rolo */
    as3514_write(AS3514_HPH_OUT_L, 0x0);

    as3514_write(AS3514_AUDIOSET1, AUDIOSET1_DAC_on);
    as3514_write(AS3514_AUDIOSET2, AUDIOSET2_SUM_off | AUDIOSET2_AGC_off | AUDIOSET2_HPH_QUALITY_LOW_POWER);
    /* common ground on, delay playback unmuting when inserting headphones */
    as3514_write(AS3514_AUDIOSET3, AUDIOSET3_HPCM_on | AUDIOSET3_HP_LONGSTART);

    as3514_write(AS3543_DAC_IF, AS3543_DAC_INT_PLL);
#ifdef SAMSUNG_YPR0
    /* Select Line 1 for FM radio */
    as3514_clear(AS3514_LINE_IN1_R, LINE_IN_R_LINE_SELECT);
#else
    /* Select Line 2 for FM radio */
    as3514_set(AS3514_LINE_IN1_R, LINE_IN_R_LINE_SELECT);
#endif
    /* Set LINEOUT to minimize pop-click noise in headphone on init stage  */
    as3514_write(AS3514_HPH_OUT_R, HPH_OUT_R_LINEOUT | HPH_OUT_R_HP_OUT_DAC);

#else
    /* as3514/as3515 */

#if defined(SANSA_E200V2) || defined(SANSA_FUZE) || defined(SANSA_C200)
    /* Set ADC off, mixer on, DAC on, line out on, line in off, mic off */
    /* Turn on SUM, DAC */
    as3514_write(AS3514_AUDIOSET1, AUDIOSET1_DAC_on | AUDIOSET1_LOUT_on | 
        AUDIOSET1_SUM_on);
#else
    /* Set ADC off, mixer on, DAC on, line out off, line in off, mic off */
    /* Turn on SUM, DAC */
    as3514_write(AS3514_AUDIOSET1, AUDIOSET1_DAC_on | AUDIOSET1_SUM_on);
#endif /* SANSA_E200V2 || SANSA_FUZE || SANSA_C200 */

    /* Set BIAS on, DITH off, AGC off, IBR_DAC max reduction, LSP_LP on, 
       IBR_LSP max reduction (50%), taken from c200v2 OF
     */
    as3514_write(AS3514_AUDIOSET2, AUDIOSET2_IBR_LSP_50 | AUDIOSET2_LSP_LP |
            AUDIOSET2_IBR_DAC_50 | AUDIOSET2_AGC_off | AUDIOSET2_DITH_off );

    /* Mute and disable speaker */
    as3514_write(AS3514_LSP_OUT_R, LSP_OUT_R_SP_OVC_TO_256MS | 0x00);
    as3514_write(AS3514_LSP_OUT_L, LSP_OUT_L_SP_MUTE | 0x00);

#ifdef PHILIPS_SA9200
    /* LRCK 8-23kHz (there are audible clicks while reading the ADC otherwise) */
    as3514_write(AS3514_PLLMODE, PLLMODE_LRCK_8_23);
#else
    /* LRCK 24-48kHz */
    as3514_write(AS3514_PLLMODE, PLLMODE_LRCK_24_48);
#endif /* PHILIPS_SA9200 */

    /* Set headphone over-current to 0, Min volume */
    as3514_write(AS3514_HPH_OUT_R, HPH_OUT_R_HP_OVC_TO_0MS | 0x00);

/* AMS Sansas based on the AS3525 need HPCM enabled, otherwise they output the
   L-R signal on both L and R headphone outputs instead of normal stereo.
   Turning it off saves a little power on targets that don't need it. */
#if (CONFIG_CPU == AS3525)
    /* Set HPCM on, ZCU off, reduce bias current, settings taken from c200v2 OF
     */
    as3514_write(AS3514_AUDIOSET3, AUDIOSET3_IBR_HPH | AUDIOSET3_ZCU_off);
#else
    /* TODO: check if AS3525 settings save power on e200v1 or as3525v2 */
    /* Set HPCM off, ZCU on */
    as3514_write(AS3514_AUDIOSET3, AUDIOSET3_HPCM_off);
#endif /* CONFIG_CPU == AS3525 */

    /* M2_Sup_off */
    as3514_set(AS3514_MIC2_L, MIC2_L_M2_SUP_off);

#endif /* HAVE_AS3543 */

    /* registers identical on as3514/as3515 and as3543 */

    /* M1_Sup_off */
    as3514_set(AS3514_MIC1_L, MIC1_L_M1_SUP_off);

    /* Headphone ON, MUTE, Min volume */
    as3514_write(AS3514_HPH_OUT_L, HPH_OUT_L_HP_ON | HPH_OUT_L_HP_MUTE | 0x00);

#if defined(SANSA_E200V2) || defined(SANSA_FUZE) || defined(SANSA_C200)
    /* Line Out Stereo, MUTE, Min volume */
    as3514_write(AS3514_LINE_OUT_L, LINE_OUT_L_LO_SES_DM_SE_ST | 
        LINE_OUT_L_LO_SES_DM_MUTE | 0x00);
#endif /* SANSA_E200V2 || SANSA_FUZE */

    /* DAC_Mute_off */
    as3514_set(AS3514_DAC_L, DAC_L_DAC_MUTE_off);

#ifdef HAVE_AS3543
    /* DAC direct - gain, mixer and limitter bypassed */
    as3514_write(AS3514_HPH_OUT_R, HPH_OUT_R_HEADPHONES | HPH_OUT_R_HP_OUT_DAC);
#endif
}

static void audiohw_mute(bool mute)
{
    if (mute) {
        as3514_set(AS3514_HPH_OUT_L, HPH_OUT_L_HP_MUTE);
#if defined(SANSA_E200V2) || defined(SANSA_FUZE) || defined(SANSA_C200)
        as3514_set(AS3514_LINE_OUT_L, LINE_OUT_L_LO_SES_DM_MUTE);
#endif /* SANSA_E200V2 || SANSA_FUZE || SANSA_C200 */
    } else {
        as3514_clear(AS3514_HPH_OUT_L, HPH_OUT_L_HP_MUTE);
#if defined(SANSA_E200V2) || defined(SANSA_FUZE) || defined(SANSA_C200)
        as3514_clear(AS3514_LINE_OUT_L, LINE_OUT_L_LO_SES_DM_MUTE);
#endif /* SANSA_E200V2 || SANSA_FUZE || SANSA_C200 */
    }
}

void audiohw_postinit(void)
{
    /* wait until outputs have stabilized */
    sleep(HZ/4);

#ifdef SANSA_E200 /* check C200 */
    /* Release pop prevention */
    GPIO_CLEAR_BITWISE(GPIOG_OUTPUT_VAL, 0x08);
#endif

#if defined(SANSA_E200V2) || defined(SANSA_FUZE) || defined(SANSA_C200)
    /* Set line out volume to 0dB */
    as3514_write_masked(AS3514_LINE_OUT_R, 0x1b, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_LINE_OUT_L, 0x1b, AS3514_VOL_MASK);
#endif /* SANSA_E200V2 || SANSA_FUZE || SANSA_C200 */

    audiohw_mute(false);
}

void audiohw_set_volume(int vol_l, int vol_r)
{
    unsigned int hph_r, hph_l;
    unsigned int mix_l, mix_r;

    /* remember volume */
    current_vol_l = vol_l;
    current_vol_r = vol_r;

    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);

    if (vol_l == 0 && vol_r == 0) {
        audiohw_mute(true);
        return;
    }

    /* We combine the mixer/DAC channel volume range with the headphone volume
     * range. We want to keep the mixers volume as high as possible and the
     * headphone volume as low as possible. */

    /* AS3543 mixer can go a little louder then the as3514, although
     * it might be possible to go louder on the as3514 as well */
#ifdef HAVE_AS3543
#define MIXER_MAX_VOLUME 0x1b /* IMPORTANT corresponds to a volume of 0dB (see below) */
#else /* lets leave the AS3514 alone until its better tested*/
#define MIXER_MAX_VOLUME 0x16
#endif

    if (vol_r <= MIXER_MAX_VOLUME) {
        mix_r = vol_r;
        hph_r = 0;
    } else {
        mix_r = MIXER_MAX_VOLUME;
        hph_r = vol_r - MIXER_MAX_VOLUME;
    }

    if (vol_l <= MIXER_MAX_VOLUME) {
        mix_l = vol_l;
        hph_l = 0;
    } else {
        mix_l = MIXER_MAX_VOLUME;
        hph_l = vol_l - MIXER_MAX_VOLUME;
    }

#ifdef HAVE_AS3543
    bool dac_only = !(as3514_regs[AS3514_AUDIOSET1] & (AUDIOSET1_ADC_on | AUDIOSET1_LIN1_on));
    if(dac_only && hph_l != 0 && hph_r != 0)
    {
        /* In DAC only mode, if both left and right volume are higher than
         * MIXER_MAX_VOLUME, we disable and bypass the DAC mixer to slightly
         * improve noise.
         *
         * WARNING this works because MIXER_MAX_VOLUME corresponds to a DAC mixer
         * volume of 0dB, thus it's the same to bypass the mixer or set its
         * level to MIXER_MAX_VOLUME, except that bypassing is less noisy */
        as3514_clear(AS3514_AUDIOSET1, AUDIOSET1_DAC_GAIN_on);
        as3514_set(AS3514_AUDIOSET2, AUDIOSET2_SUM_off);
        as3514_write_masked(AS3514_HPH_OUT_R, HPH_OUT_R_HP_OUT_DAC, HPH_OUT_R_HP_OUT_MASK);
    }
    else
    {
        /* In all other cases, we have no choice but to go through the main mixer
         * (aka SUM) to get the volume we want or to properly route audio from
         * line-in/microphone. */
        as3514_set(AS3514_AUDIOSET1, AUDIOSET1_DAC_GAIN_on);
        as3514_clear(AS3514_AUDIOSET2, AUDIOSET2_SUM_off);
        as3514_write_masked(AS3514_HPH_OUT_R, HPH_OUT_R_HP_OUT_SUM, HPH_OUT_R_HP_OUT_MASK);
    }
#endif

    as3514_write_masked(AS3514_DAC_R, mix_r, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_DAC_L, mix_l, AS3514_VOL_MASK);
#if defined(HAVE_RECORDING) || defined(HAVE_FMRADIO_IN)
    as3514_write_masked(AS3514_LINE_IN_R, mix_r, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_LINE_IN_L, mix_l, AS3514_VOL_MASK);
#endif
    as3514_write_masked(AS3514_HPH_OUT_R, hph_r, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_HPH_OUT_L, hph_l, AS3514_VOL_MASK);

    audiohw_mute(false);
}

#if 0 /* unused */
void audiohw_set_lineout_volume(int vol_l, int vol_r)
{
#ifdef HAVE_AS3543
    /* line out volume is set in the same registers */
    audiohw_set_volume(vol_l, vol_r);
#else
    vol_l = vol_tenthdb2hw(vol_l);
    vol_r = vol_tenthdb2hw(vol_r);
    as3514_write_masked(AS3514_LINE_OUT_R, vol_r, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_LINE_OUT_L, vol_l, AS3514_VOL_MASK);
#endif
}
#endif

/* Nice shutdown of AS3514 audio codec */
void audiohw_close(void)
{
    /* mute headphones */
    audiohw_mute(true);

#ifdef SANSA_E200 /* check C200 */
    /* Set pop prevention */
    GPIO_SET_BITWISE(GPIOG_OUTPUT_VAL, 0x08);
#endif

    /* turn on common */
    as3514_clear(AS3514_AUDIOSET3, AUDIOSET3_HPCM_off);

    /* turn off everything */
    as3514_clear(AS3514_HPH_OUT_L, HPH_OUT_L_HP_ON);
    as3514_write(AS3514_AUDIOSET1, 0x0);

    /* Allow caps to discharge */
    sleep(HZ/4);
}

void audiohw_set_frequency(int fsel)
{
#if defined(SANSA_E200) || defined(SANSA_C200)
    if ((unsigned)fsel >= HW_NUM_FREQ)
        fsel = HW_FREQ_DEFAULT;

    as3514_write(AS3514_PLLMODE, hw_freq_sampr[fsel] < 24000 ?
                 PLLMODE_LRCK_8_23 : PLLMODE_LRCK_24_48);

    audiohw_set_sampr_dividers(fsel);
#endif
    (void)fsel;
}

#if defined(HAVE_RECORDING)
void audiohw_enable_recording(bool source_mic)
{
    if (source_mic) {
        /* ADCmux = Stereo Microphone */
        as3514_write_masked(AS3514_ADC_R, ADC_R_ADCMUX_ST_MIC,
                            ADC_R_ADCMUX);

        /* MIC1_on, others off */
        as3514_write_masked(AS3514_AUDIOSET1, AUDIOSET1_MIC1_on, 
                            AUDIOSET1_INPUT_MASK);

#if CONFIG_CPU == AS3525v2
        /* XXX: why is the microphone supply not needed on other models ?? */
        /* Enable supply */
        as3514_clear(AS3514_MIC1_L, MIC1_L_M1_SUP_off);
#endif

        /* M1_AGC_off */
        as3514_clear(AS3514_MIC1_R, MIC1_R_M1_AGC_off);
    } else {
        /* ADCmux = Line_IN1 or Line_IN2 */
        as3514_write_masked(AS3514_ADC_R, ADC_R_ADCMUX_LINE_IN,
                            ADC_R_ADCMUX);

        /* LIN1_or LIN2 on, rest off */
        as3514_write_masked(AS3514_AUDIOSET1, AUDIOSET1_LIN_on,
                            AUDIOSET1_INPUT_MASK);

#if CONFIG_CPU == AS3525v2
        /* Disable supply */
        as3514_set(AS3514_MIC1_L, MIC1_L_M1_SUP_off);
#endif
    }

    /* ADC_Mute_off */
    as3514_set(AS3514_ADC_L, ADC_L_ADC_MUTE_off);
    /* ADC_on */
    as3514_set(AS3514_AUDIOSET1, AUDIOSET1_ADC_on);
}

void audiohw_disable_recording(void)
{
    /* ADC_Mute_on */
    as3514_clear(AS3514_ADC_L, ADC_L_ADC_MUTE_off);

    /* ADC_off, all input sources off */
    as3514_clear(AS3514_AUDIOSET1, AUDIOSET1_ADC_on | AUDIOSET1_INPUT_MASK);
}

/**
 * Set recording volume
 *
 * Line in   : 0 .. 23 .. 31 =>
               Volume -34.5 .. +00.0 .. +12.0 dB
 * Mic (left): 0 .. 23 .. 39 =>
 *             Volume -34.5 .. +00.0 .. +24.0 dB
 *
 */
void audiohw_set_recvol(int left, int right, int type)
{
    switch (type)
    {
    case AUDIO_GAIN_MIC:
    {
        /* Combine MIC gains seamlessly with ADC levels */
        unsigned int mic1_r;

        if (left >= 36) {
            /* M1_Gain = +40db, ADR_Vol = +7.5dB .. +12.0 dB =>
               +19.5 dB .. +24.0 dB */
            left -= 8;
            mic1_r = MIC1_R_M1_GAIN_40DB;
        } else if (left >= 32) {
            /* M1_Gain = +34db, ADR_Vol = +7.5dB .. +12.0 dB =>
               +13.5 dB .. +18.0 dB */
            left -= 4; 
            mic1_r = MIC1_R_M1_GAIN_34DB;
        } else {
            /* M1_Gain = +28db, ADR_Vol = -34.5dB .. +12.0 dB =>
               -34.5 dB .. +12.0 dB */
            mic1_r = MIC1_R_M1_GAIN_28DB;
        }

        right = left;

        as3514_write_masked(AS3514_MIC1_R, mic1_r, MIC1_R_M1_GAIN);
        break;
        }
    case AUDIO_GAIN_LINEIN:
        break;
    default:
        return;
    }

    as3514_write_masked(AS3514_ADC_R, right, AS3514_VOL_MASK);
    as3514_write_masked(AS3514_ADC_L, left, AS3514_VOL_MASK);
}
#endif /* HAVE_RECORDING */

#if defined(HAVE_RECORDING) || defined(HAVE_FMRADIO_IN)
/**
 * Enable line in analog monitoring
 *
 */
void audiohw_set_monitor(bool enable)
{
    /* On AS3543 we play with DAC mixer bypass to decrease noise. This means that
     * even in DAC mode, the headphone mux might be set to HPH_OUT_R_HP_OUT_SUM or
     * HPH_OUT_R_HP_OUT_DAC depending on the volume. Care must be taken when
     * changing monitor.
     *
     * The only safe procedure is to first change the Audioset1 register to enable/disable
     * monitor, then call audiohw_set_volume to recompute the audio routing, then
     * mute/unmute lines-in. */
    if (enable) {
        /* select either LIN1 or LIN2 but keep them muted for now */
        as3514_write_masked(AS3514_AUDIOSET1, AUDIOSET1_LIN_on,
                            AUDIOSET1_LIN1_on | AUDIOSET1_LIN2_on);
        /* change audio routing */
        audiohw_set_volume(current_vol_l, current_vol_r);
        /* finally unmute line-in */
        as3514_set(AS3514_LINE_IN_R, LINE_IN1_R_LI1R_MUTE_off);
        as3514_set(AS3514_LINE_IN_L, LINE_IN1_L_LI1L_MUTE_off);
    }
    else {
        /* mute line-in */
        as3514_clear(AS3514_LINE_IN_R, LINE_IN1_R_LI1R_MUTE_off);
        as3514_clear(AS3514_LINE_IN_L, LINE_IN1_L_LI1L_MUTE_off);
        /* disable line-in */
        as3514_clear(AS3514_AUDIOSET1, AUDIOSET1_LIN1_on | AUDIOSET1_LIN2_on);
        /* change audio routing */
        audiohw_set_volume(current_vol_l, current_vol_r);
    }
}
#endif /* HAVE_RECORDING || HAVE_FMRADIO_IN */
