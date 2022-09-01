/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2008 by Maurus Cuelenaere
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

#ifndef __TOUCHSCREEN_INCLUDE_H_
#define __TOUCHSCREEN_INCLUDE_H_

struct touchscreen_calibration
{
    int x[3][2];
    int y[3][2];
};

struct touchscreen_parameter
{
    int A, B, C, D, E, F;
    int divider;
};

enum touchscreen_mode
{
    TOUCHSCREEN_POINT = 0, /* touchscreen returns pixel co-ords */
    TOUCHSCREEN_BUTTON,    /* touchscreen returns BUTTON_* area codes
                              actual pixel value will still be accessible
                              from button_get_data */
};

extern struct touchscreen_parameter calibration_parameters;
extern const struct touchscreen_parameter default_calibration_parameters;
int touchscreen_calibrate(struct touchscreen_calibration *cal);
int touchscreen_to_pixels(int x, int y, int *data);
void touchscreen_set_mode(enum touchscreen_mode mode);
enum touchscreen_mode touchscreen_get_mode(void);
void touchscreen_disable_mapping(void);
void touchscreen_reset_mapping(void);
int touchscreen_get_scroll_threshold(void);
void touchscreen_enable(bool en);
#ifndef HAS_BUTTON_HOLD
void touchscreen_enable_device(bool en);
bool touchscreen_is_enabled(void);
#endif

#endif /* __TOUCHSCREEN_INCLUDE_H_ */
