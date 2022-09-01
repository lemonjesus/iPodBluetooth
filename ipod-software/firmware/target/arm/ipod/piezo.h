/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006-2007 Robert Keevil
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

void piezo_init(void);
void piezo_play(unsigned short inv_freq, unsigned char form);
void piezo_play_for_tick(unsigned short inv_freq,
                    unsigned char form, unsigned int dur);
void piezo_play_for_usec(unsigned short inv_freq,
                    unsigned char form, unsigned int dur);
void piezo_stop(void);
void piezo_clear(void);
bool piezo_busy(void);
unsigned int piezo_hz(unsigned int hz);
void piezo_button_beep(bool beep, bool force);
