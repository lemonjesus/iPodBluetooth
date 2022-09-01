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
#ifndef __ATA_H__
#define __ATA_H__

#include <stdbool.h>
#include "config.h" /* for HAVE_MULTIVOLUME or not */
#include "mv.h" /* for IF_MV() and friends */

#ifdef HAVE_ATA_SMART
/* S.M.A.R.T. headers from smartmontools-5.42 */
#define NUMBER_ATA_SMART_ATTRIBUTES     30

struct ata_smart_attribute {
  unsigned char id;
  /* meaning of flag bits: see MACROS just below */
  /* WARNING: MISALIGNED! */
  unsigned short flags;
  unsigned char current;
  unsigned char worst;
  unsigned char raw[6];
  unsigned char reserv;
} __attribute__((packed));

/* MACROS to interpret the flags bits in the previous structure. */
/* These have not been implemented using bitflags and a union, to make */
/* it portable across bit/little endian and different platforms. */

/* 0: Prefailure bit */

/* From SFF 8035i Revision 2 page 19: Bit 0 (pre-failure/advisory bit) */
/* - If the value of this bit equals zero, an attribute value less */
/* than or equal to its corresponding attribute threshold indicates an */
/* advisory condition where the usage or age of the device has */
/* exceeded its intended design life period. If the value of this bit */
/* equals one, an attribute value less than or equal to its */
/* corresponding attribute threshold indicates a prefailure condition */
/* where imminent loss of data is being predicted. */
#define ATTRIBUTE_FLAGS_PREFAILURE(x) (x & 0x01)

/* 1: Online bit  */

/* From SFF 8035i Revision 2 page 19: Bit 1 (on-line data collection */
/* bit) - If the value of this bit equals zero, then the attribute */
/* value is updated only during off-line data collection */
/* activities. If the value of this bit equals one, then the attribute */
/* value is updated during normal operation of the device or during */
/* both normal operation and off-line testing. */
#define ATTRIBUTE_FLAGS_ONLINE(x) (x & 0x02)

/* The following are (probably) IBM's, Maxtors and  Quantum's definitions for the */
/* vendor-specific bits: */
/* 2: Performance type bit */
#define ATTRIBUTE_FLAGS_PERFORMANCE(x) (x & 0x04)

/* 3: Errorrate type bit */
#define ATTRIBUTE_FLAGS_ERRORRATE(x) (x & 0x08)

/* 4: Eventcount bit */
#define ATTRIBUTE_FLAGS_EVENTCOUNT(x) (x & 0x10)

/* 5: Selfpereserving bit */
#define ATTRIBUTE_FLAGS_SELFPRESERVING(x) (x & 0x20)

/* 6-15: Reserved for future use */
#define ATTRIBUTE_FLAGS_OTHER(x) ((x) & 0xffc0)

struct ata_smart_values
{
    unsigned short int revnumber;
    struct ata_smart_attribute vendor_attributes [NUMBER_ATA_SMART_ATTRIBUTES];
    unsigned char offline_data_collection_status;
    unsigned char self_test_exec_status;
    unsigned short int total_time_to_complete_off_line;
    unsigned char vendor_specific_366;
    unsigned char offline_data_collection_capability;
    unsigned short int smart_capability;
    unsigned char errorlog_capability;
    unsigned char vendor_specific_371;
    unsigned char short_test_completion_time;
    unsigned char extend_test_completion_time;
    unsigned char conveyance_test_completion_time;
    unsigned char reserved_375_385[11];
    unsigned char vendor_specific_386_510[125];
    unsigned char chksum;
} __attribute__((packed));

/* Raw attribute value print formats */
enum ata_attr_raw_format
{
    RAWFMT_DEFAULT,
    RAWFMT_RAW8,
    RAWFMT_RAW16,
    RAWFMT_RAW48,
    RAWFMT_HEX48,
    RAWFMT_RAW64,
    RAWFMT_HEX64,
    RAWFMT_RAW16_OPT_RAW16,
    RAWFMT_RAW16_OPT_AVG16,
    RAWFMT_RAW24_DIV_RAW24,
    RAWFMT_RAW24_DIV_RAW32,
    RAWFMT_SEC2HOUR,
    RAWFMT_MIN2HOUR,
    RAWFMT_HALFMIN2HOUR,
    RAWFMT_MSEC24_HOUR32,
    RAWFMT_TEMPMINMAX,
    RAWFMT_TEMP10X,
};
#endif /* HAVE_ATA_SMART */

struct storage_info;

void ata_enable(bool on);
void ata_spindown(int seconds);
void ata_sleep(void);
void ata_sleepnow(void);
/* NOTE: DO NOT use this to poll for disk activity.
         If you are waiting for the disk to become active before
         doing something use ata_idle_notify.h
 */
bool ata_disk_is_active(void);
int ata_soft_reset(void);
int ata_init(void) STORAGE_INIT_ATTR;
void ata_close(void);
int ata_read_sectors(IF_MD(int drive,) unsigned long start, int count, void* buf);
int ata_write_sectors(IF_MD(int drive,) unsigned long start, int count, const void* buf);
void ata_spin(void);
#if (CONFIG_LED == LED_REAL)
void ata_set_led_enabled(bool enabled);
#endif
unsigned short* ata_get_identify(void);

#ifdef STORAGE_GET_INFO
void ata_get_info(IF_MD(int drive,) struct storage_info *info);
#endif
#ifdef HAVE_HOTSWAP
bool ata_removable(IF_MD_NONVOID(int drive));
bool ata_present(IF_MD_NONVOID(int drive));
#endif


#ifdef CONFIG_STORAGE_MULTI
int ata_num_drives(int first_drive);
#endif


long ata_last_disk_activity(void);
int ata_spinup_time(void); /* ticks */

#ifdef HAVE_ATA_DMA
/* Returns current DMA mode */
int ata_get_dma_mode(void);
#endif /* HAVE_ATA_DMA */

#ifdef HAVE_ATA_SMART
/* Returns current S.M.A.R.T. data */
int ata_read_smart(struct ata_smart_values*);
#endif

#endif /* __ATA_H__ */
