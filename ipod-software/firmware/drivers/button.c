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
#include "system.h"
#include "button.h"
#include "kernel.h"
#include "thread.h"
#include "backlight.h"
#include "serial.h"
#include "power.h"
#include "powermgmt.h"
#ifdef HAVE_SDL
#include "button-sdl.h"
#else
#include "button-target.h"
#endif
#ifdef HAVE_REMOTE_LCD
#include "lcd-remote.h"
#endif

struct event_queue button_queue SHAREDBSS_ATTR;

static long lastbtn;   /* Last valid button status */
static long last_read; /* Last button status, for debouncing/filtering */
static intptr_t button_data; /* data value from last message dequeued */
#ifdef HAVE_LCD_BITMAP
static bool flipped;  /* buttons can be flipped to match the LCD flip */
#endif
#ifdef HAVE_BACKLIGHT
static bool filter_first_keypress;
#ifdef HAVE_REMOTE_LCD
static bool remote_filter_first_keypress;
#endif
#endif /* HAVE_BACKLIGHT */
#ifdef HAVE_HEADPHONE_DETECTION
static bool phones_present = false;
#endif

/* how long until repeat kicks in, in centiseconds */
#define REPEAT_START      (30*HZ/100)

#ifndef HAVE_TOUCHSCREEN
/* the next two make repeat "accelerate", which is nice for lists
 * which begin to scroll a bit faster when holding until the
 * real list accerelation kicks in (this smoothes acceleration)
 */

/* the speed repeat starts at, in centiseconds */
#define REPEAT_INTERVAL_START   (16*HZ/100)
/* speed repeat finishes at, in centiseconds */
#define REPEAT_INTERVAL_FINISH  (5*HZ/100)
#else
/*
 * on touchscreen it's different, scrolling is done by swiping over the
 * screen (potentially very quickly) and is completely different from button
 * targets
 * So, on touchscreen we don't want to artifically slow down early repeats,
 * it'd have the contrary effect of making rockbox appear lagging
 */
#define REPEAT_INTERVAL_START   (5*HZ/100)
#define REPEAT_INTERVAL_FINISH  (5*HZ/100)
#endif

#ifdef HAVE_BUTTON_DATA
static int button_read(int *data);
static int lastdata = 0;
#else
static int button_read(void);
#endif

#ifdef HAVE_TOUCHSCREEN
static int last_touchscreen_touch;
#endif    
#if defined(HAVE_HEADPHONE_DETECTION)
static struct timeout hp_detect_timeout; /* Debouncer for headphone plug/unplug */
/* This callback can be used for many different functions if needed -
   just check to which object tmo points */
static int btn_detect_callback(struct timeout *tmo)
{
    /* Try to post only transistions */
    const long id = tmo->data ? SYS_PHONE_PLUGGED : SYS_PHONE_UNPLUGGED;
    queue_remove_from_head(&button_queue, id);
    queue_post(&button_queue, id, 0);
    return 0;
}
#endif

static bool button_try_post(int button, int data)
{
#ifdef HAVE_TOUCHSCREEN
    /* one can swipe over the scren very quickly,
     * for this to work we want to forget about old presses and
     * only respect the very latest ones */
    const bool force_post = true;
#else
    /* Only post events if the queue is empty,
     * to avoid afterscroll effects.
     * i.e. don't post new buttons if previous ones haven't been
     * processed yet - but always post releases */
    const bool force_post = button & BUTTON_REL;
#endif

    bool ret = queue_empty(&button_queue);
    if (!ret && force_post)
    {
        queue_remove_from_head(&button_queue, button);
        ret = true;
    }

    if (ret)
        queue_post(&button_queue, button, data);

    /* on touchscreen we posted unconditionally */
    return ret;
}

static void button_tick(void)
{
    static int count = 0;
    static int repeat_speed = REPEAT_INTERVAL_START;
    static int repeat_count = 0;
    static bool repeat = false;
    static bool post = false;
#ifdef HAVE_BACKLIGHT
    static bool skip_release = false;
#ifdef HAVE_REMOTE_LCD
    static bool skip_remote_release = false;
#endif
#endif
    int diff;
    int btn;
#ifdef HAVE_BUTTON_DATA
    int data = 0;
#else
    const int data = 0;
#endif

#if defined(HAS_SERIAL_REMOTE) && !defined(SIMULATOR)
    /* Post events for the remote control */
    btn = remote_control_rx();
    if(btn)
        button_try_post(btn, 0);
#endif

#ifdef HAVE_BUTTON_DATA
    btn = button_read(&data);
#else
    btn = button_read();
#endif
#if defined(HAVE_HEADPHONE_DETECTION)
    if (headphones_inserted() != phones_present)
    {
        /* Use the autoresetting oneshot to debounce the detection signal */
        phones_present = !phones_present;
        timeout_register(&hp_detect_timeout, btn_detect_callback,
                         HZ/2, phones_present);
    }
#endif

    /* Find out if a key has been released */
    diff = btn ^ lastbtn;
    if(diff && (btn & diff) == 0)
    {
#ifdef HAVE_BACKLIGHT
#ifdef HAVE_REMOTE_LCD
        if(diff & BUTTON_REMOTE)
            if(!skip_remote_release)
                button_try_post(BUTTON_REL | diff, data);
            else
                skip_remote_release = false;
        else
#endif
            if(!skip_release)
                button_try_post(BUTTON_REL | diff, data);
            else
                skip_release = false;
#else
        button_try_post(BUTTON_REL | diff, data);
#endif
    }
    else
    {
        if ( btn )
        {
            /* normal keypress */
            if ( btn != lastbtn )
            {
                post = true;
                repeat = false;
                repeat_speed = REPEAT_INTERVAL_START;
            }
            else /* repeat? */
            {

#if defined(DX50) || defined(DX90)
                /*
                    Power button on these devices reports two distinct key codes, which are
                    triggerd by a short or medium duration press. Additionlly a long duration press
                    will trigger a hard reset, which is hardwired.

                    The time delta between medium and long duration press is not large enough to
                    register here as power off repeat. A hard reset is triggered before Rockbox
                    can power off.

                    To cirumvent the hard reset, Rockbox will shutdown on the first POWEROFF_BUTTON
                    repeat. POWEROFF_BUTTON is associated with the a medium duration press of the
                    power button.
                */
                if(btn & POWEROFF_BUTTON)
                {
                    sys_poweroff();
                }
#endif

                if ( repeat )
                {
                    if (!post)
                        count--;
                    if (count == 0) {
                        post = true;
                        /* yes we have repeat */
                        if (repeat_speed > REPEAT_INTERVAL_FINISH)
                            repeat_speed--;
                        count = repeat_speed;

                        repeat_count++;

                        /* Send a SYS_POWEROFF event if we have a device
                           which doesn't shut down easily with the OFF
                           key */
#ifdef HAVE_SW_POWEROFF
                        if ((btn & POWEROFF_BUTTON
#ifdef RC_POWEROFF_BUTTON
                                    || btn == RC_POWEROFF_BUTTON
#endif
                                    ) &&
#if CONFIG_CHARGING && !defined(HAVE_POWEROFF_WHILE_CHARGING)
                                !charger_inserted() &&
#endif
                                repeat_count > POWEROFF_COUNT)
                        {
                            /* Tell the main thread that it's time to
                               power off */
                            sys_poweroff();

                            /* Safety net for players without hardware
                               poweroff */
#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
                            if(repeat_count > POWEROFF_COUNT * 10)
                                power_off();
#endif
                        }
#endif
                    }
                }
                else
                {
                    if (count++ > REPEAT_START)
                    {
                        post = true;
                        repeat = true;
                        repeat_count = 0;
                        /* initial repeat */
                        count = REPEAT_INTERVAL_START;
                    }
#ifdef HAVE_TOUCHSCREEN
                    else if (lastdata != data && btn == lastbtn)
                    {   /* only coordinates changed, post anyway */
                        if (touchscreen_get_mode() == TOUCHSCREEN_POINT)
                            post = true;
                    }
#endif
                }
            }
            if ( post )
            {
                if (repeat)
                {
                    /* Only post repeat events if the queue is empty,
                     * to avoid afterscroll effects. */
                    if (button_try_post(BUTTON_REPEAT | btn, data))
                    {
#ifdef HAVE_BACKLIGHT
#ifdef HAVE_REMOTE_LCD
                        skip_remote_release = false;
#endif
                        skip_release = false;
#endif
                        post = false;
                    }
                }
                else
                {
#ifdef HAVE_BACKLIGHT
#ifdef HAVE_REMOTE_LCD
                    if (btn & BUTTON_REMOTE) {
                        if (!remote_filter_first_keypress 
                            || is_remote_backlight_on(false)
#if defined(IRIVER_H100_SERIES) || defined(IRIVER_H300_SERIES)
                            || (remote_type()==REMOTETYPE_H300_NONLCD)
#endif
                            )
                            button_try_post(btn, data);
                        else
                            skip_remote_release = true;
                    }
                    else
#endif
                        if (!filter_first_keypress || is_backlight_on(false)
#if BUTTON_REMOTE
                                || (btn & BUTTON_REMOTE)
#endif
                           )
                            button_try_post(btn, data);
                        else
                            skip_release = true;
#else /* no backlight, nothing to skip */
                    button_try_post(btn, data);
#endif
                    post = false;
                }
#ifdef HAVE_REMOTE_LCD
                if(btn & BUTTON_REMOTE)
                    remote_backlight_on();
                else
#endif
                {
                    backlight_on();
#ifdef HAVE_BUTTON_LIGHT
                    buttonlight_on();
#endif
                }

                reset_poweroff_timer();
            }
        }
        else
        {
            repeat = false;
            count = 0;
        }
    }
    lastbtn = btn & ~(BUTTON_REL | BUTTON_REPEAT);
#ifdef HAVE_BUTTON_DATA
    lastdata = data;
#endif
}

#ifdef HAVE_ADJUSTABLE_CPU_FREQ
static bool button_boosted = false;
static long button_unboost_tick;
#define BUTTON_UNBOOST_TMO HZ

static void button_boost(bool state)
{
    if (state)
    {
        button_unboost_tick = current_tick + BUTTON_UNBOOST_TMO;

        if (!button_boosted)
        {
            button_boosted = true;
            cpu_boost(true);
        }
    }
    else if (!state && button_boosted)
    {
        button_boosted = false;
        cpu_boost(false);
    }
}

static void button_queue_wait(struct queue_event *evp, int timeout)
{
    /* Loop once after wait time if boosted in order to unboost and wait the
       full remaining time */
    do
    {
        int ticks = timeout;

        if (ticks == 0) /* TIMEOUT_NOBLOCK */
            ;
        else if (ticks > 0)
        {
            if (button_boosted && ticks > BUTTON_UNBOOST_TMO)
                ticks = BUTTON_UNBOOST_TMO;

            timeout -= ticks;
        }
        else            /* TIMEOUT_BLOCK (ticks < 0) */
        {
            if (button_boosted)
                ticks = BUTTON_UNBOOST_TMO;
        }

        queue_wait_w_tmo(&button_queue, evp, ticks);
        if (evp->id != SYS_TIMEOUT)
        {
            /* GUI boost build gets immediate kick, otherwise at least 3
               messages had to be there */
        #ifndef HAVE_GUI_BOOST
            if (queue_count(&button_queue) >= 2)
        #endif
                button_boost(true);

            break; 
        }

        if (button_boosted && TIME_AFTER(current_tick, button_unboost_tick))
            button_boost(false);
    }
    while (timeout);
}
#else /* ndef HAVE_ADJUSTABLE_CPU_FREQ */
static inline void button_queue_wait(struct queue_event *evp, int timeout)
{
    queue_wait_w_tmo(&button_queue, evp, timeout);
}
#endif /* HAVE_ADJUSTABLE_CPU_FREQ */

int button_queue_count( void )
{
    return queue_count(&button_queue);
}

long button_get(bool block)
{
    struct queue_event ev;
    button_queue_wait(&ev, block ? TIMEOUT_BLOCK : TIMEOUT_NOBLOCK);

    if (ev.id == SYS_TIMEOUT)
        ev.id = BUTTON_NONE;
    else
        button_data = ev.data;

    return ev.id;
}

long button_get_w_tmo(int ticks)
{
    struct queue_event ev;    
    button_queue_wait(&ev, ticks);

    if (ev.id == SYS_TIMEOUT)
        ev.id = BUTTON_NONE;
    else
        button_data = ev.data;

    return ev.id;
}

intptr_t button_get_data(void)
{
    return button_data;
}

void button_init(void)
{
    /* Init used objects first */
    queue_init(&button_queue, true);

#ifdef HAVE_BUTTON_DATA
    int temp;
#endif
    /* hardware inits */
    button_init_device();

#ifdef HAVE_BUTTON_DATA
    button_read(&temp);
    lastbtn = button_read(&temp);
#else
    button_read();
    lastbtn = button_read();
#endif
    
    reset_poweroff_timer();

#ifdef HAVE_LCD_BITMAP
    flipped = false;
#endif
#ifdef HAVE_BACKLIGHT
    filter_first_keypress = false;
#ifdef HAVE_REMOTE_LCD
    remote_filter_first_keypress = false;
#endif    
#endif
#ifdef HAVE_TOUCHSCREEN
    last_touchscreen_touch = 0xffff;
#endif    
    /* Start polling last */
    tick_add_task(button_tick);
}

#ifdef BUTTON_DRIVER_CLOSE
void button_close(void)
{
    tick_remove_task(button_tick);
}
#endif /* BUTTON_DRIVER_CLOSE */

#ifdef HAVE_LCD_FLIP
/*
 * helper function to swap LEFT/RIGHT, UP/DOWN (if present), and F1/F3 (Recorder)
 */
static int button_flip(int button)
{
    int newbutton = button;

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
    newbutton &=
        ~(BUTTON_LEFT | BUTTON_RIGHT
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
        | BUTTON_UP | BUTTON_DOWN
#endif
#if defined(BUTTON_SCROLL_BACK) && defined(BUTTON_SCROLL_FWD)
        | BUTTON_SCROLL_BACK | BUTTON_SCROLL_FWD
#endif
#if CONFIG_KEYPAD == RECORDER_PAD
        | BUTTON_F1 | BUTTON_F3
#endif
#if (CONFIG_KEYPAD == SANSA_C200_PAD) || (CONFIG_KEYPAD == SANSA_CLIP_PAD) ||\
    (CONFIG_KEYPAD == GIGABEAT_PAD) || (CONFIG_KEYPAD == GIGABEAT_S_PAD)
        | BUTTON_VOL_UP | BUTTON_VOL_DOWN
#endif
#if CONFIG_KEYPAD == PHILIPS_SA9200_PAD
        | BUTTON_VOL_UP | BUTTON_VOL_DOWN
        | BUTTON_NEXT | BUTTON_PREV
#endif
        );

    if (button & BUTTON_LEFT)
        newbutton |= BUTTON_RIGHT;
    if (button & BUTTON_RIGHT)
        newbutton |= BUTTON_LEFT;
#if defined(BUTTON_UP) && defined(BUTTON_DOWN)
    if (button & BUTTON_UP)
        newbutton |= BUTTON_DOWN;
    if (button & BUTTON_DOWN)
        newbutton |= BUTTON_UP;
#endif
#if defined(BUTTON_SCROLL_BACK) && defined(BUTTON_SCROLL_FWD)
    if (button & BUTTON_SCROLL_BACK)
        newbutton |= BUTTON_SCROLL_FWD;
    if (button & BUTTON_SCROLL_FWD)
        newbutton |= BUTTON_SCROLL_BACK;
#endif
#if CONFIG_KEYPAD == RECORDER_PAD
    if (button & BUTTON_F1)
        newbutton |= BUTTON_F3;
    if (button & BUTTON_F3)
        newbutton |= BUTTON_F1;
#endif
#if (CONFIG_KEYPAD == SANSA_C200_PAD) || (CONFIG_KEYPAD == SANSA_CLIP_PAD) ||\
    (CONFIG_KEYPAD == GIGABEAT_PAD) || (CONFIG_KEYPAD == GIGABEAT_S_PAD)
    if (button & BUTTON_VOL_UP)
        newbutton |= BUTTON_VOL_DOWN;
    if (button & BUTTON_VOL_DOWN)
        newbutton |= BUTTON_VOL_UP;
#endif
#if CONFIG_KEYPAD == PHILIPS_SA9200_PAD
    if (button & BUTTON_VOL_UP)
        newbutton |= BUTTON_VOL_DOWN;
    if (button & BUTTON_VOL_DOWN)
        newbutton |= BUTTON_VOL_UP;
    if (button & BUTTON_NEXT)
        newbutton |= BUTTON_PREV;
    if (button & BUTTON_PREV)
        newbutton |= BUTTON_NEXT;
#endif
#endif /* !SIMULATOR */
    return newbutton;
}

/*
 * set the flip attribute
 * better only call this when the queue is empty
 */
void button_set_flip(bool flip)
{
    if (flip != flipped) /* not the current setting */
    {
        /* avoid race condition with the button_tick() */
        int oldlevel = disable_irq_save();
        lastbtn = button_flip(lastbtn);
        flipped = flip;
        restore_irq(oldlevel);
    }
}
#endif /* HAVE_LCD_FLIP */

#ifdef HAVE_BACKLIGHT
void set_backlight_filter_keypress(bool value)
{
    filter_first_keypress = value;
}
#ifdef HAVE_REMOTE_LCD
void set_remote_backlight_filter_keypress(bool value)
{
    remote_filter_first_keypress = value;
}
#endif
#endif

/*
 * Get button pressed from hardware
 */
#ifdef HAVE_BUTTON_DATA
static int button_read(int *data)
{
    int btn = button_read_device(data);
#else
static int button_read(void)
{
    int btn = button_read_device();
#endif
    int retval;

#ifdef HAVE_LCD_FLIP
    if (btn && flipped)
        btn = button_flip(btn); /* swap upside down */
#endif /* HAVE_LCD_FLIP */

#ifdef HAVE_TOUCHSCREEN
    if (btn & BUTTON_TOUCHSCREEN)
        last_touchscreen_touch = current_tick;
#endif        
    /* Filter the button status. It is only accepted if we get the same
       status twice in a row. */
#ifndef HAVE_TOUCHSCREEN
    if (btn != last_read)
            retval = lastbtn;
    else
#endif
        retval = btn;
    last_read = btn;

    return retval;
}

int button_status(void)
{
    return lastbtn;
}

#ifdef HAVE_BUTTON_DATA
int button_status_wdata(int *pdata)
{
    *pdata = lastdata;
    return lastbtn;
}
#endif

void button_clear_queue(void)
{
    queue_clear(&button_queue);
}

#ifdef HAVE_TOUCHSCREEN
int touchscreen_last_touch(void)
{
    return last_touchscreen_touch;
}
#endif

#ifdef HAVE_WHEEL_ACCELERATION
/* WHEEL_ACCEL_FACTOR = 2^16 / WHEEL_ACCEL_START */
#define WHEEL_ACCEL_FACTOR (1<<16)/WHEEL_ACCEL_START
/**
 * data:
 *    [31] Use acceleration
 * [30:24] Message post count (skipped + 1) (1-127)
 *  [23:0] Velocity - degree/sec
 *
 * WHEEL_ACCEL_FACTOR:
 * Value in degree/sec -- configurable via settings -- above which 
 * the accelerated scrolling starts. Factor is internally scaled by 
 * 1<<16 in respect to the following 32bit integer operations.
 */
int button_apply_acceleration(const unsigned int data)
{
    int delta = (data >> 24) & 0x7f;

    if ((data & (1 << 31)) != 0)
    {
        /* read driver's velocity from data */
        unsigned int v = data & 0xffffff;

        /* v = 28.4 fixed point */
        v = (WHEEL_ACCEL_FACTOR * v)>>(16-4);

        /* Calculate real numbers item to scroll based upon acceleration
         * setting, use correct roundoff */
#if   (WHEEL_ACCELERATION == 1)
        v = (v*v     + (1<< 7))>> 8;
#elif (WHEEL_ACCELERATION == 2)
        v = (v*v*v   + (1<<11))>>12;
#elif (WHEEL_ACCELERATION == 3)
        v = (v*v*v*v + (1<<15))>>16;
#endif

        if (v > 1)
            delta *= v;
    }

    return delta;
}
#endif /* HAVE_WHEEL_ACCELERATION */

#if (defined(HAVE_TOUCHPAD) || defined(HAVE_TOUCHSCREEN)) && !defined(HAS_BUTTON_HOLD)
void button_enable_touch(bool en)
{
#ifdef HAVE_TOUCHPAD
    touchpad_enable(en);
#endif
#ifdef HAVE_TOUCHSCREEN
    touchscreen_enable(en);
#endif
}
#endif
