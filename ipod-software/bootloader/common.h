/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 by Linus Nielsen Feltzing
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

/* Set this to true to enable lcd_update() in the printf function */
extern bool verbose;

/* Error types */
#define     EATA                    -1
#define     EDISK                   -2
#define     EBOOTFILE               -3

/* Functions common to all bootloaders */
void reset_screen(void);
int printf(const char *format, ...);
void error(int errortype, int error, bool shutdown);
int load_raw_firmware(unsigned char* buf, char* firmware, int buffer_size);
#ifdef ROCKBOX_HAS_LOGF
void display_logf(void);
#endif
