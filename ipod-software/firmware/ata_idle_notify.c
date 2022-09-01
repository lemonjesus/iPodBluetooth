/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 Jonathan Gordon
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
#include <stdbool.h>
#include "system.h"
#include "ata.h"
#include "ata_idle_notify.h"
#include "kernel.h"
#include "string.h"

#if USING_STORAGE_CALLBACK
static void wrapper(unsigned short id, void *ev_data, void *user_data)
{
    (void)id;
    (void)ev_data;
    void (*func)(void) = user_data;
    func();
}
#endif

void register_storage_idle_func(void (*function)(void))
{
#if USING_STORAGE_CALLBACK
    add_event_ex(DISK_EVENT_SPINUP, true, wrapper, function);
#else
    function(); /* just call the function now */
/* this _may_ cause problems later if the calling function
   sets a variable expecting the callback to unset it, because
   the callback will be run before this function exits, so before the var is set */
#endif
}

#if USING_STORAGE_CALLBACK
void unregister_storage_idle_func(void (*func)(void), bool run)
{
    remove_event_ex(DISK_EVENT_SPINUP, wrapper, func);
    
    if (run)
        func();
}

bool call_storage_idle_notifys(bool force)
{
    static int lock_until = 0;

    if (!force)
    {
        if (TIME_BEFORE(current_tick,lock_until) )
            return false;
    }
    lock_until = current_tick + 30*HZ;

    send_event(DISK_EVENT_SPINUP, NULL);
    
    return true;
}
#endif
