/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Rockbox driver for iPod LCDs
 *
 * Based on code from the ipodlinux project - http://ipodlinux.org/
 * Adapted for Rockbox in November 2005
 *
 * Original file: linux/arch/armnommu/mach-ipod/fb.c
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
#include "config.h"
#include "cpu.h"
#include "lcd.h"
#include "kernel.h"
#include "system.h"
#include "hwcompat.h"

/* LCD command codes for HD66753 */

#define R_START_OSC             0x00
#define R_DRV_OUTPUT_CONTROL    0x01
#define R_DRV_WAVEFORM_CONTROL  0x02
#define R_POWER_CONTROL         0x03
#define R_CONTRAST_CONTROL      0x04
#define R_ENTRY_MODE            0x05
#define R_ROTATION              0x06
#define R_DISPLAY_CONTROL       0x07
#define R_CURSOR_CONTROL        0x08
#define R_HORIZONTAL_CURSOR_POS 0x0b
#define R_VERTICAL_CURSOR_POS   0x0c
#define R_1ST_SCR_DRV_POS       0x0d
#define R_2ND_SCR_DRV_POS       0x0e
#define R_RAM_WRITE_MASK        0x10
#define R_RAM_ADDR_SET          0x11
#define R_RAM_DATA              0x12

#ifdef HAVE_BACKLIGHT_INVERSION
/* The backlight makes the LCD appear negative on the 1st/2nd gen */
static bool lcd_inverted = false;
static bool lcd_backlit = false;
#if NUM_CORES > 1
/* invert_display() and the lcd_blit_* functions need to be corelocked */
static struct corelock cl IBSS_ATTR;
#endif
static void invert_display(void);
#endif

#if defined(IPOD_1G2G) || defined(IPOD_3G)
#define POWER_REG_H 0x1120   /* 1/7 Bias, 5x step-up @ clk/8 */
#else
#define POWER_REG_H 0x1200   /* 1/7 Bias, 6x step-up @ clk/32 */
#endif

#define CONTRAST_REG_H 0x400

#if defined(IPOD_1G2G)
#define DEFAULT_CONTRAST 45
#elif defined(IPOD_3G)
#define DEFAULT_CONTRAST 50
#elif defined(IPOD_MINI) || defined(IPOD_MINI2G)
#define DEFAULT_CONTRAST 42
#elif defined(IPOD_4G)
#define DEFAULT_CONTRAST 35
#endif

/* needed for flip */
static int addr_offset;
#if defined(IPOD_MINI) || defined(IPOD_MINI2G)
static int pix_offset;
void lcd_write_data_shifted(const fb_data* p_bytes, int count);
#endif

/* wait for LCD with timeout */
static inline void lcd_wait_write(void)
{
    while (LCD1_CONTROL & LCD1_BUSY_MASK);
}

/* send LCD command */
static void lcd_prepare_cmd(unsigned cmd)
{
    lcd_wait_write();
#ifdef IPOD_MINI2G
    LCD1_CMD = cmd | 0x740000;
#else
    LCD1_CMD = 0;
    lcd_wait_write();
    LCD1_CMD = cmd;
#endif
}

/* send LCD command and data */
static void lcd_cmd_and_data(unsigned cmd, unsigned data)
{
    lcd_wait_write();
#ifdef IPOD_MINI2G
    LCD1_CMD = cmd | 0x740000;
    lcd_wait_write();
    LCD1_CMD = data | 0x760000;
#else
    LCD1_CMD = 0;
    lcd_wait_write();
    LCD1_CMD = cmd;
    lcd_wait_write();
    LCD1_DATA = data >> 8;
    lcd_wait_write();
    LCD1_DATA = data & 0xff;
#endif
}

/* LCD init */
void lcd_init_device(void)
{
#if (NUM_CORES > 1) && defined(HAVE_BACKLIGHT_INVERSION)
    corelock_init(&cl);
#endif
#ifdef IPOD_MINI2G /* serial LCD hookup */
    lcd_wait_write();
    LCD1_CONTROL = 0x01730084; /* fastest setting */
#elif defined(IPOD_1G2G) || defined(IPOD_3G)
    LCD1_CONTROL = (LCD1_CONTROL & 0x0002) | 0x0084; 
                   /* fastest setting, keep backlight bit */
#else
    LCD1_CONTROL = 0x0084; /* fastest setting */
#endif

    lcd_cmd_and_data(R_DRV_WAVEFORM_CONTROL, 0x48);
                     /* C waveform, no EOR, 9 lines inversion */
    lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H | 0xc);
    lcd_cmd_and_data(R_DISPLAY_CONTROL, 0x0019);
    lcd_set_contrast(DEFAULT_CONTRAST);
#ifdef HAVE_BACKLIGHT_INVERSION
    invert_display();
#endif
    lcd_set_flip(false);
    lcd_cmd_and_data(R_ENTRY_MODE, 0x0000);
}

/*** hardware configuration ***/

int lcd_default_contrast(void)
{
    return DEFAULT_CONTRAST;
}

/* Rockbox stores the contrast as 0..63 - we add 64 to it */
void lcd_set_contrast(int val)
{
    if (val < 0) val = 0;
    else if (val > 63) val = 63;

    lcd_cmd_and_data(R_CONTRAST_CONTROL, CONTRAST_REG_H | (val + 64));
}

#ifdef HAVE_BACKLIGHT_INVERSION
static void invert_display(void)
{
    static bool last_invert = false;
    bool new_invert = lcd_inverted ^ lcd_backlit;

    if (new_invert != last_invert)
    {
        int oldlevel = disable_irq_save();
#if NUM_CORES > 1
        corelock_lock(&cl);
        lcd_cmd_and_data(R_DISPLAY_CONTROL, new_invert? 0x0027 : 0x0019);
        corelock_unlock(&cl);
#else
        lcd_cmd_and_data(R_DISPLAY_CONTROL, new_invert? 0x0027 : 0x0019);
#endif
        restore_irq(oldlevel);
        last_invert = new_invert;
    }
}

void lcd_set_invert_display(bool yesno)
{
    lcd_inverted = yesno;
    invert_display();
}

void lcd_set_backlight_inversion(bool yesno)
{
    lcd_backlit = yesno;
    invert_display();
}
#else
void lcd_set_invert_display(bool yesno)
{
    lcd_cmd_and_data(R_DISPLAY_CONTROL, yesno ? 0x0027 : 0x0019);
}
#endif

/* turn the display upside down (call lcd_update() afterwards) */
void lcd_set_flip(bool yesno)
{
#if defined(IPOD_MINI) || defined(IPOD_MINI2G)
    if (yesno) 
    {    /* 168x112, inverse COM order */
        lcd_cmd_and_data(R_DRV_OUTPUT_CONTROL, 0x020d);
        lcd_cmd_and_data(R_1ST_SCR_DRV_POS, 0x8316);    /* 22..131 */
        addr_offset = (22 << 5) | (20 - 4);
        pix_offset = -2;
    }
    else 
    {   /* 168x112,  inverse SEG order */
        lcd_cmd_and_data(R_DRV_OUTPUT_CONTROL, 0x010d);
        lcd_cmd_and_data(R_1ST_SCR_DRV_POS, 0x6d00);    /* 0..109 */
        addr_offset = 20;
        pix_offset = 0;
    }
#else
    if (yesno) 
    {   /* 168x128, inverse SEG & COM order */
        lcd_cmd_and_data(R_DRV_OUTPUT_CONTROL, 0x030f);
        lcd_cmd_and_data(R_1ST_SCR_DRV_POS, 0x8304);    /* 4..131 */
        addr_offset = (4 << 5) | (20 - 1);
    } 
    else 
    {   /* 168x128 */
        lcd_cmd_and_data(R_DRV_OUTPUT_CONTROL, 0x000f);
        lcd_cmd_and_data(R_1ST_SCR_DRV_POS, 0x7f00);    /* 0..127 */
        addr_offset = 20;
    }
#endif
}

#ifdef HAVE_LCD_ENABLE
void lcd_enable(bool on)
{
    if (on)
    {
        lcd_cmd_and_data(R_START_OSC, 1);               /* start oscillation */
        sleep(HZ/10);                                           /* wait 10ms */
        lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H); /*clear standby mode */
        lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H | 0xc);
                                                   /* enable opamp & booster */
    }
    else
    {
        lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H);
                                               /* switch off opamp & booster */
        lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H | 0x1);
                                                       /* enter standby mode */
    }
}
#endif /* HAVE_LCD_ENABLE */

/*** update functions ***/

/* Helper function. */
void lcd_mono_data(const unsigned char *data, int count);

/* Performance function that works with an external buffer
   note that x, bwidtht and stride are in 8-pixel units! */
void lcd_blit_mono(const unsigned char *data, int bx, int y, int bwidth,
                   int height, int stride)
{
#if (NUM_CORES > 1) && defined(HAVE_BACKLIGHT_INVERSION)
    corelock_lock(&cl);
#endif
    while (height--)
    {
        lcd_cmd_and_data(R_RAM_ADDR_SET, (y++ << 5) + addr_offset - bx);
        lcd_prepare_cmd(R_RAM_DATA);
        lcd_mono_data(data, bwidth);
        data += stride;
    }
#if (NUM_CORES > 1) && defined(HAVE_BACKLIGHT_INVERSION)
    corelock_unlock(&cl);
#endif
}

/* Helper function for lcd_grey_phase_blit(). */
void lcd_grey_data(unsigned char *values, unsigned char *phases, int count);

/* Performance function that works with an external buffer
   note that bx and bwidth are in 8-pixel units! */
void lcd_blit_grey_phase(unsigned char *values, unsigned char *phases,
                         int bx, int y, int bwidth, int height, int stride)
{
#if (NUM_CORES > 1) && defined(HAVE_BACKLIGHT_INVERSION)
    corelock_lock(&cl);
#endif
    while (height--)
    {
        lcd_cmd_and_data(R_RAM_ADDR_SET, (y++ << 5) + addr_offset - bx);
        lcd_prepare_cmd(R_RAM_DATA);
        lcd_grey_data(values, phases, bwidth);
        values += stride;
        phases += stride;
    }
#if (NUM_CORES > 1) && defined(HAVE_BACKLIGHT_INVERSION)
    corelock_unlock(&cl);
#endif
}

void lcd_update_rect(int x, int y, int width, int height)
{
    int xmax, ymax;

    if (x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (width <= 0)
        return;
    
    ymax = y + height - 1;
    if (ymax >= LCD_HEIGHT)
        ymax = LCD_HEIGHT - 1;

#if defined(IPOD_MINI) || defined(IPOD_MINI2G)
    x += pix_offset;
#endif
     /* writing is done in 16-bit units (8 pixels) */
    xmax = (x + width - 1) >> 3;
    x >>= 3;
    width = xmax - x + 1;

    for (; y <= ymax; y++) 
    {
        lcd_cmd_and_data(R_RAM_ADDR_SET, (y << 5) + addr_offset - x);
        lcd_prepare_cmd(R_RAM_DATA);

#if defined(IPOD_MINI) || defined(IPOD_MINI2G)
        if (pix_offset == -2)
            lcd_write_data_shifted(FBADDR(2*x, y), width);
        else
#endif
            lcd_write_data(FBADDR(2*x, y), width);
    }
}

/* Update the display. */
void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

#ifdef HAVE_LCD_SHUTDOWN
/* LCD powerdown */
void lcd_shutdown(void)
{
    lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H | 0x00); /* Turn off op amp power */
    lcd_cmd_and_data(R_POWER_CONTROL, POWER_REG_H | 0x02); /* Put LCD driver in standby */
}
#endif
