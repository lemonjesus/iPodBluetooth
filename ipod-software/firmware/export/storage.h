/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Alan Korr
 * Copyright (C) 2008 by Frank Gevaerts
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
#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <stdbool.h>
#include "config.h" /* for HAVE_MULTIDRIVE or not */
#include "mv.h"

#if (CONFIG_STORAGE & STORAGE_HOSTFS) || defined(SIMULATOR)
#define HAVE_HOSTFS
#endif

#if (CONFIG_STORAGE & STORAGE_SD)
#include "sd.h"
#endif
#if (CONFIG_STORAGE & STORAGE_MMC)
#include "mmc.h"
#endif
#if (CONFIG_STORAGE & STORAGE_ATA)
#include "ata.h"
#endif
#if (CONFIG_STORAGE & STORAGE_NAND)
#include "nand.h"
#endif
#if (CONFIG_STORAGE & STORAGE_RAMDISK)
#include "ramdisk.h"
#endif

struct storage_info
{
    unsigned int sector_size;
    unsigned int num_sectors;
    char *vendor;
    char *product;
    char *revision;
};

#ifdef HAVE_HOSTFS
#include "hostfs.h"
/* stubs for the plugin api */
static inline void stub_storage_sleep(void) {}
static inline void stub_storage_spin(void) {}
static inline void stub_storage_spindown(int timeout) { (void)timeout; }
#endif

#if !defined(CONFIG_STORAGE_MULTI) || defined(HAVE_HOSTFS)
/* storage_spindown, storage_sleep and storage_spin are passed as
 * pointers, which doesn't work with argument-macros.
 */
    #define storage_num_drives() NUM_DRIVES
    #if defined(HAVE_HOSTFS)
        #define STORAGE_FUNCTION(NAME) (stub_## NAME)
        #define storage_spindown stub_storage_spindown
        #define storage_sleep stub_storage_sleep
        #define storage_spin stub_storage_spin

        #define storage_enable(on)
        #define storage_sleepnow()
        #define storage_disk_is_active() 0
        #define storage_soft_reset()
        #define storage_init() hostfs_init()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() hostfs_flush()
        #endif
        #define storage_last_disk_activity() (-1)
        #define storage_spinup_time() 0
        #define storage_get_identify() (NULL) /* not actually called anywher */

        #ifdef STORAGE_GET_INFO
            #error storage_get_info not implemented
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) hostfs_removable(IF_MD(drive))
            #define storage_present(drive) hostfs_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) hostfs_driver_type(IF_MV(drive))
    #elif (CONFIG_STORAGE & STORAGE_ATA)
        #define STORAGE_FUNCTION(NAME) (ata_## NAME)
        #define storage_spindown ata_spindown
        #define storage_sleep ata_sleep
        #define storage_spin ata_spin

        #define storage_enable(on) ata_enable(on)
        #define storage_sleepnow() ata_sleepnow()
        #define storage_disk_is_active() ata_disk_is_active()
        #define storage_soft_reset() ata_soft_reset()
        #define storage_init() ata_init()
        #define storage_close() ata_close()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() (void)0
        #endif
        #define storage_last_disk_activity() ata_last_disk_activity()
        #define storage_spinup_time() ata_spinup_time()
        #define storage_get_identify() ata_get_identify()

        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) ata_get_info(IF_MD(drive,) info)
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) ata_removable(IF_MD(drive))
            #define storage_present(drive) ata_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) (STORAGE_ATA_NUM)
    #elif (CONFIG_STORAGE & STORAGE_SD)
        #define STORAGE_FUNCTION(NAME) (sd_## NAME)
        #define storage_spindown sd_spindown
        #define storage_sleep sd_sleep
        #define storage_spin sd_spin

        #define storage_enable(on) sd_enable(on)
        #define storage_sleepnow() sd_sleepnow()
        #define storage_disk_is_active() 0
        #define storage_soft_reset() (void)0
        #define storage_init() sd_init()
        #define storage_close() sd_close()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() (void)0
        #endif
        #define storage_last_disk_activity() sd_last_disk_activity()
        #define storage_spinup_time() 0
        #define storage_get_identify() sd_get_identify()

        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) sd_get_info(IF_MD(drive,) info)
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) sd_removable(IF_MD(drive))
            #define storage_present(drive) sd_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) (STORAGE_SD_NUM)
     #elif (CONFIG_STORAGE & STORAGE_MMC)
        #define STORAGE_FUNCTION(NAME) (mmc_## NAME)
        #define storage_spindown mmc_spindown
        #define storage_sleep mmc_sleep
        #define storage_spin mmc_spin

        #define storage_enable(on) mmc_enable(on)
        #define storage_sleepnow() mmc_sleepnow()
        #define storage_disk_is_active() mmc_disk_is_active()
        #define storage_soft_reset() (void)0
        #define storage_init() mmc_init()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() (void)0
        #endif
        #define storage_last_disk_activity() mmc_last_disk_activity()
        #define storage_spinup_time() 0
        #define storage_get_identify() mmc_get_identify()
       
        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) mmc_get_info(IF_MD(drive,) info)
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) mmc_removable(IF_MD(drive))
            #define storage_present(drive) mmc_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) (STORAGE_MMC_NUM)
    #elif (CONFIG_STORAGE & STORAGE_NAND)
        #define STORAGE_FUNCTION(NAME) (nand_## NAME)
        #define storage_spindown nand_spindown
        #define storage_sleep nand_sleep
        #define storage_spin nand_spin

        #define storage_enable(on) (void)0
        #define storage_sleepnow() nand_sleepnow()
        #define storage_disk_is_active() 0
        #define storage_soft_reset() (void)0
        #define storage_init() nand_init()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() nand_flush()
        #endif
        #define storage_last_disk_activity() nand_last_disk_activity()
        #define storage_spinup_time() 0
        #define storage_get_identify() nand_get_identify()
       
        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) nand_get_info(IF_MD(drive,) info)
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) nand_removable(IF_MD(drive))
            #define storage_present(drive) nand_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) (STORAGE_NAND_NUM)
    #elif (CONFIG_STORAGE & STORAGE_RAMDISK)
        #define STORAGE_FUNCTION(NAME) (ramdisk_## NAME)
        #define storage_spindown ramdisk_spindown
        #define storage_sleep ramdisk_sleep
        #define storage_spin ramdisk_spin

        #define storage_enable(on) (void)0
        #define storage_sleepnow() ramdisk_sleepnow()
        #define storage_disk_is_active() 0
        #define storage_soft_reset() (void)0
        #define storage_init() ramdisk_init()
        #ifdef HAVE_STORAGE_FLUSH
            #define storage_flush() (void)0
        #endif
        #define storage_last_disk_activity() ramdisk_last_disk_activity()
        #define storage_spinup_time() 0
        #define storage_get_identify() ramdisk_get_identify()
       
        #ifdef STORAGE_GET_INFO
            #define storage_get_info(drive, info) ramdisk_get_info(IF_MD(drive,) info)
        #endif
        #ifdef HAVE_HOTSWAP
            #define storage_removable(drive) ramdisk_removable(IF_MD(drive))
            #define storage_present(drive) ramdisk_present(IF_MD(drive))
        #endif
        #define storage_driver_type(drive) (STORAGE_RAMDISK_NUM)
    #else
        //#error No storage driver!
    #endif
#else /* CONFIG_STORAGE_MULTI || !HAVE_HOSTFS */

/* Multi-driver use normal functions */

void storage_enable(bool on);
void storage_sleep(void);
void storage_sleepnow(void);
bool storage_disk_is_active(void);
int storage_soft_reset(void);
int storage_init(void) STORAGE_INIT_ATTR;
int storage_flush(void);
void storage_spin(void);
void storage_spindown(int seconds);
long storage_last_disk_activity(void);
int storage_spinup_time(void);
int storage_num_drives(void);
#ifdef STORAGE_GET_INFO
void storage_get_info(int drive, struct storage_info *info);
#endif
#ifdef HAVE_HOTSWAP
bool storage_removable(int drive);
bool storage_present(int drive);
#endif
int storage_driver_type(int drive);

#endif /* NOT CONFIG_STORAGE_MULTI and NOT SIMULATOR*/

int storage_read_sectors(IF_MD(int drive,) unsigned long start, int count, void* buf);
int storage_write_sectors(IF_MD(int drive,) unsigned long start, int count, const void* buf);
#endif
