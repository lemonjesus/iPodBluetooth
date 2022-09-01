/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Björn Stenberg
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
#ifndef _DISK_H_
#define _DISK_H_

#include "config.h"
#include "mv.h" /* for volume definitions */

struct partinfo
{
    unsigned long start; /* first sector (LBA) */
    unsigned long size;  /* number of sectors */
    unsigned char type;
};

#define PARTITION_TYPE_FAT32                0x0b
#define PARTITION_TYPE_FAT32_LBA            0x0c
#define PARTITION_TYPE_FAT16                0x06
#define PARTITION_TYPE_OS2_HIDDEN_C_DRIVE   0x84

bool disk_init(IF_MD_NONVOID(int drive));
bool disk_partinfo(int partition, struct partinfo *info);

int disk_mount_all(void); /* returns the # of successful mounts */
int disk_mount(int drive);
int disk_unmount_all(void);
int disk_unmount(int drive);

/* The number of 512-byte sectors in a "logical" sector. Needed for ipod 5.5G */
#ifdef MAX_LOG_SECTOR_SIZE
int disk_get_sector_multiplier(IF_MD_NONVOID(int drive));
#endif

bool disk_present(IF_MD_NONVOID(int drive));

#endif /* _DISK_H_ */
