/***************************************************************************
*             __________               __   ___.
*   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
*   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
*   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
*   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
*                     \/            \/     \/    \/            \/
* $Id$
*
* Copyright (C) 2015 Thomas Jarosch
*
* Loosely based upon rbcodecplatform-unix.h from rbcodec
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

#ifndef _COMMON_UNITTEST_H
#define _COMMON_UNITTEST_H

/* debugf, logf */
#define debugf(...) fprintf(stderr, __VA_ARGS__)

#ifndef logf
#define logf(...) do { fprintf(stderr, __VA_ARGS__); \
                       putc('\n', stderr);           \
                  } while (0)
#endif

#ifndef panicf
#define panicf(...) do { fprintf(stderr, __VA_ARGS__); \
                         putc('\n', stderr);           \
                         exit(-1);                     \
                  } while (0)
#endif

#endif /* _COMMON_UNITTEST_H */
