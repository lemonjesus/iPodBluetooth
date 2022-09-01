/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2003 Tat Tang
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

#ifndef _FONT_CACHE_H_
#define _FONT_CACHE_H_
#include <stdbool.h>
#include "lru.h"

/*******************************************************************************
 * 
 ******************************************************************************/
struct font_cache
{
    struct lru _lru;
    int _size;
    int _capacity;
    int _prev_char_code;
    int _prev_result;
    short *_index; /* index of lru handles in char_code order */
};

struct font_cache_entry
{
    unsigned short _char_code;
    unsigned char width;
    unsigned char bitmap[1]; /* place holder */
};

/* void (*f) (void*, struct font_cache_entry*); */
/* Create an auto sized font cache from buf */
void font_cache_create(
    struct font_cache* fcache, void* buf, int buf_size, int bitmap_bytes_size);

/* Get font cache entry for the given char_code
 *
 * cache_only: true if only a cache lookup should be performed and loading on miss should be avoided
 *
 * Note: With cache_only this can return NULL, which otherwise never happens */
struct font_cache_entry* font_cache_get(
    struct font_cache* fcache,
    unsigned short char_code,
    bool cache_only,
    void (*callback) (struct font_cache_entry* p, void *callback_data),
    void *callback_data);

#endif
