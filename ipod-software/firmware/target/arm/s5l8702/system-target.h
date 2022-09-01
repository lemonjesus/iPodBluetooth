/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: system-target.h 28791 2010-12-11 09:39:33Z Buschel $
 *
 * Copyright (C) 2007 by Dave Chapman
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
#ifndef SYSTEM_TARGET_H
#define SYSTEM_TARGET_H

#include "system-arm.h"
#include "mmu-arm.h"

#include "s5l8702.h"    // USEC_TIMER

#define CPUFREQ_SLEEP   32768
#if (CONFIG_CPU == S5L8702)
#define CPUFREQ_MAX     216000000
#define CPUFREQ_DEFAULT 54000000
#define CPUFREQ_NORMAL  54000000

#elif (CONFIG_CPU == S5L8720)
#define CPUFREQ_MAX     266000000
#define CPUFREQ_DEFAULT 133000000
#define CPUFREQ_NORMAL  133000000
#endif

#define STORAGE_WANTS_ALIGN

#define S5L8702_UNCACHED_ADDR(a) ((typeof(a)) ((uintptr_t)(a) + 0x40000000))
#define S5L8702_PHYSICAL_ADDR(a) ((typeof(a)) ((uintptr_t)(a)))


#define inl(a)    (*(volatile unsigned long *) (a))
#define outl(a,b) (*(volatile unsigned long *) (b) = (a))
#define inb(a)    (*(volatile unsigned char *) (a))
#define outb(a,b) (*(volatile unsigned char *) (b) = (a))
#define inw(a)    (*(volatile unsigned short*) (a))
#define outw(a,b) (*(volatile unsigned short*) (b) = (a))

static inline void udelay(unsigned usecs)       // TODO: a timer.c ???, aunque lo cierto es que TIME_BEFORE esta definido en system.h
{
    unsigned stop = USEC_TIMER + usecs;
    while (TIME_BEFORE(USEC_TIMER, stop));
}

void usb_insert_int(void);
void usb_remove_int(void);

#ifdef BOOTLOADER
void system_preinit(void);
#endif

#endif /* SYSTEM_TARGET_H */
