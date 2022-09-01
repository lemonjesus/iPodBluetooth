/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Daniel Stenberg
 *
 * iPod driver based on code from the ipodlinux project - http://ipodlinux.org
 * Adapted for Rockbox in December 2005
 * Original file: linux/arch/armnommu/mach-ipod/keyboard.c
 * Copyright (c) 2003-2005 Bernard Leach (leachbj@bouncycastle.org)
 *
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

/*
 * Rockbox button functions
 */

#include <stdlib.h>
#include "config.h"
#include "cpu.h"
#include "system.h"
#include "button.h"
#include "kernel.h"
#include "backlight.h"
#include "serial.h"
#include "power.h"
#include "powermgmt.h"

#define WHEELCLICKS_PER_ROTATION  96
#define WHEEL_BASE_SENSITIVITY     6 /* Compute every ... clicks */
#define WHEEL_REPEAT_VELOCITY     45 /* deg/s */
#define WHEEL_SMOOTHING_VELOCITY 100 /* deg/s */

/* Variable to use for setting button status in interrupt handler */
int int_btn = BUTTON_NONE;

static void handle_scroll_wheel(int new_scroll)
{
    static const signed char scroll_state[4][4] = {
        {0, 1, -1, 0},
        {-1, 0, 0, 1},
        {1, 0, 0, -1},
        {0, -1, 1, 0}
    };

    static int prev_scroll = -1;
    static int direction = 0;
    static int count = 0;
    static long nextbacklight_hw_on = 0;

    int wheel_keycode = BUTTON_NONE;
    int scroll;

    static unsigned long wheel_delta = 1ul << 24;
    static unsigned long wheel_velocity = 0;
    static unsigned long last_wheel_usec = 0;
    static int prev_keypost = BUTTON_NONE;

    unsigned long usec;
    unsigned long v;

    if ( prev_scroll == -1 ) {
        prev_scroll = new_scroll;
        return;
    }
    
    scroll = scroll_state[prev_scroll][new_scroll];
    prev_scroll = new_scroll;

    if (direction != scroll) {
        /* direction reversal or was hold - reset all */
        direction = scroll;
        count = 0;
        prev_keypost = BUTTON_NONE;
        wheel_velocity = 0;
        wheel_delta = 1ul << 24;
        return;
    }

   /* poke backlight every 1/4s of activity */
    if (TIME_AFTER(current_tick, nextbacklight_hw_on)) {
        backlight_on();
        reset_poweroff_timer();
        nextbacklight_hw_on = current_tick + HZ/4;
    }

    if (++count < WHEEL_BASE_SENSITIVITY)
        return;

    count = 0;
    /* Mini 1st Gen wheel has inverse direction mapping
     * compared to 1st..3rd Gen wheel. */
    switch (direction) {
        case 1:
            wheel_keycode = BUTTON_SCROLL_FWD;
            break;
        case -1:
            wheel_keycode = BUTTON_SCROLL_BACK;
            break;
        default:
            /* only happens if we get out of sync */
            break;
    }

    /* have a keycode */

    usec = USEC_TIMER;
    v = usec - last_wheel_usec;

    /* calculate deg/s based upon sensitivity-adjusted interrupt period */

    if ((long)v <= 0) {
        /* timer wrapped (no activity for awhile), skip acceleration */
        v = 0;
        wheel_delta = 1ul << 24;
    }
    else {
        if (v > 0xfffffffful/WHEELCLICKS_PER_ROTATION) {
            v = 0xfffffffful/WHEELCLICKS_PER_ROTATION; /* check overflow below */
        }

        v = 360000000ul*WHEEL_BASE_SENSITIVITY / (v*WHEELCLICKS_PER_ROTATION);

        if (v > 0xfffffful)
            v = 0xfffffful; /* limit to 24 bits */
    }

    if (v < WHEEL_SMOOTHING_VELOCITY) {
        /* very slow - no smoothing */
        wheel_velocity = v;
        /* ensure backlight never gets stuck for an extended period if tick
         * wrapped such that next poke is very far ahead */
        nextbacklight_hw_on = current_tick - 1;
    }
    else {
        /* some velocity filtering to smooth things out */
        wheel_velocity = (7*wheel_velocity + v) / 8;
    }

    if (queue_empty(&button_queue)) {
        int key = wheel_keycode;

        if (v >= WHEEL_REPEAT_VELOCITY && prev_keypost == key) {
            /* quick enough and same key is being posted more than once in a
             * row - generate repeats - use unsmoothed v to guage */
            key |= BUTTON_REPEAT;
        }

        prev_keypost = wheel_keycode;

        /* post wheel keycode with wheel data */
        queue_post(&button_queue, key,
                   (wheel_velocity >= WHEEL_ACCEL_START ? (1ul << 31) : 0)
                    | wheel_delta | wheel_velocity);
        /* message posted - reset delta */
        wheel_delta = 1ul << 24;
    }
    else {
        /* skipped post - increment delta and limit to 7 bits */
        wheel_delta += 1ul << 24;

        if (wheel_delta > (0x7ful << 24))
            wheel_delta = 0x7ful << 24;
    }

    last_wheel_usec = usec;
}

/* mini 1 only, mini 2G uses iPod 4G code */
static int ipod_mini_button_read(void)
{
    unsigned char source, wheel_source, state, wheel_state;
    int btn = BUTTON_NONE;

    /* The ipodlinux source had a udelay(250) here, but testing has shown that
       it is not needed - tested on mini 1g. */
    /* udelay(250);*/

    /* get source(s) of interupt */
    source = GPIOA_INT_STAT & 0x3f;
    wheel_source = GPIOB_INT_STAT & 0x30;

    if (source == 0 && wheel_source == 0) {
        return BUTTON_NONE; /* not for us */
    }

    /* get current keypad & wheel status */
    state = GPIOA_INPUT_VAL & 0x3f;
    wheel_state = GPIOB_INPUT_VAL & 0x30;

    /* toggle interrupt level */
    GPIOA_INT_LEV = ~state;
    GPIOB_INT_LEV = ~wheel_state;

   /* ack any active interrupts */
    if (source)
        GPIOA_INT_CLR = source;
    if (wheel_source)
        GPIOB_INT_CLR = wheel_source;

   if (button_hold())
        return BUTTON_NONE;

    /* hold switch causes all outputs to go low    */
    /* we shouldn't interpret these as key presses */
        if (!(state & 0x1))
            btn |= BUTTON_SELECT;
        if (!(state & 0x2))
            btn |= BUTTON_MENU;
        if (!(state & 0x4))
            btn |= BUTTON_PLAY;
        if (!(state & 0x8))
            btn |= BUTTON_RIGHT;
        if (!(state & 0x10))
            btn |= BUTTON_LEFT;

        if (wheel_source & 0x30) {
            handle_scroll_wheel((wheel_state & 0x30) >> 4);
        }

    return btn;
}

void ipod_mini_button_int(void)
{
    CPU_HI_INT_DIS = GPIO0_MASK;
    int_btn = ipod_mini_button_read();
    //CPU_INT_EN = 0x40000000;
    CPU_HI_INT_EN = GPIO0_MASK;
}

void button_init_device(void)
{
    /* iPod Mini G1 */
    /* buttons - enable as input */
    GPIOA_ENABLE |= 0x3f;
    GPIOA_OUTPUT_EN &= ~0x3f;
    /* scroll wheel- enable as input */
    GPIOB_ENABLE |= 0x30; /* port b 4,5 */
    GPIOB_OUTPUT_EN &= ~0x30; /* port b 4,5 */
    /* buttons - set interrupt levels */
    GPIOA_INT_LEV = ~(GPIOA_INPUT_VAL & 0x3f);
    GPIOA_INT_CLR = GPIOA_INT_STAT & 0x3f;
    /* scroll wheel - set interrupt levels */
    GPIOB_INT_LEV = ~(GPIOB_INPUT_VAL & 0x30);
    GPIOB_INT_CLR = GPIOB_INT_STAT & 0x30;
    /* enable interrupts */
    GPIOA_INT_EN = 0x3f;
    GPIOB_INT_EN = 0x30;
    /* unmask interrupt */
    CPU_INT_EN = 0x40000000;
    CPU_HI_INT_EN = GPIO0_MASK;
}

/*
 * Get button pressed from hardware
 */
int button_read_device(void)
{
    static bool hold_button = false;
    bool hold_button_old;

    /* normal buttons */
    hold_button_old = hold_button;
    hold_button = button_hold();

    if (hold_button != hold_button_old)
        backlight_hold_changed(hold_button);

    /* The int_btn variable is set in the button interrupt handler */
    return int_btn;
}

bool button_hold(void)
{
    return (GPIOA_INPUT_VAL & 0x20)?false:true;
}

bool headphones_inserted(void)
{
    return (GPIOA_INPUT_VAL & 0x80)?true:false;
}
