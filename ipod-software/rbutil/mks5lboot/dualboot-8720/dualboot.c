/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2017 by Cástor Muñoz
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
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "system.h"
#include "button.h"

#include "s5l8702.h"
#include "clocking-s5l8702.h"
#include "spi-s5l8702.h"
#include "norboot-target.h"
#include "piezo.h"


#define OF_LOADADDR     IRAM1_ORIG

/* tone sequences: period (uS), duration (ms), silence (ms) */
static uint16_t alive[] = { 500,100,0, 0 };
// static uint16_t alive1000[] = { 500,1000,0, 0 };
static uint16_t happy[] = { 1000,100,0, 500,150,0, 0 };
static uint16_t fatal[] = { 3000,500,500, 3000,500,500, 3000,500,0, 0 };
#define sad2 (&fatal[3])
#define sad  (&fatal[6])

#if 0
/* iPod Classic: decrypted hashes for known OFs */
static unsigned char of_sha[][SIGN_SZ] = {
// TODO
};
#define N_OF (int)(sizeof(of_sha)/SIGN_SZ)

/* If the firmware can not be identified then we assume that
   it is a RB bootloader */
#define FW_RB   N_OF

static int identify_fw(struct Im3Info *hinfo)
{
    unsigned char hash[SIGN_SZ];
    int of;

    /* decrypt hash to identify OF */
    memcpy(hash, hinfo->u.enc12.data_sign, SIGN_SZ);
    hwkeyaes(HWKEYAES_DECRYPT, HWKEYAES_UKEY, hash, SIGN_SZ);

    for (of = 0; of < N_OF; of++)
        if (memcmp(hash, of_sha[of], SIGN_SZ) == 0)
            break;

    return of;
}
#endif

#ifdef DUALBOOT_UNINSTALL
/* Uninstall RB bootloader */
void main(void)
{
    uint16_t *status = happy;
#if 0
    struct Im3Info *hinfo;
    void *fw_addr;
    unsigned bl_nor_sz;
#endif

    usec_timer_init();
    piezo_seq(alive);
#if 0
    spi_clkdiv(SPI_PORT, 4); /* SPI clock = 27/5 MHz. */

    hinfo = (struct Im3Info*)OF_LOADADDR;
    fw_addr = (void*)hinfo + IM3HDR_SZ;

    if (im3_read(NORBOOT_OFF, hinfo, NULL) != 0) {
        status = sad;
        goto bye; /* no FW found */
    }

    if (identify_fw(hinfo) != FW_RB) {
        status = happy;
        goto bye; /* RB bootloader not installed, nothing to do */
    }

    /* if this FW is a RB bootloader, OF should be located just behind it */
    bl_nor_sz = im3_nor_sz(hinfo);
    if ((im3_read(NORBOOT_OFF + bl_nor_sz, hinfo, fw_addr) != 0) ||
                            (identify_fw(hinfo) == FW_RB)) {
        status = sad;
        goto bye; /* OF not found */
    }

    /* decrypted OF correctly loaded, encrypt it before restoration */
    im3_crypt(HWKEYAES_ENCRYPT, hinfo, fw_addr);

    /* restore OF to it's original place */
    if (!im3_write(NORBOOT_OFF, hinfo)) {
        status = fatal;
        goto bye; /* corrupted NOR, use iTunes to restore */
    }

    /* erase freed NOR blocks */
    bootflash_init(SPI_PORT);
    bootflash_erase_blocks(SPI_PORT,
            (NORBOOT_OFF + im3_nor_sz(hinfo)) >> 12, bl_nor_sz >> 12);
    bootflash_close(SPI_PORT);

bye:
#endif

    /* minimum time between the initial and the final beeps */
    while (USEC_TIMER < 2000000);
    piezo_seq(status);
    WDTCON = 0x100000; /* WDT reset */
    while (1);
}

#else
/* Install RB bootloader */
struct Im3Info bl_hinfo __attribute__((section(".im3info.data"))) =
{
    .ident = IM3_IDENT,
    .version = IM3_VERSION,
    .enc_type = 2,
};

static uint32_t get_uint32le(unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

void main(void)
{
    uint16_t *status = happy;
#if 0
    int single_boot;
    struct Im3Info *hinfo;
    void *fw_addr;
    unsigned bl_nor_sz;
#endif

    usec_timer_init();
    piezo_seq(alive);

//     spi_clkdiv(SPI_PORT, 4); /* SPI clock = 27/5 MHz. */

    /* check for DFU_OPT_NONPERSISTENT option */
    if (bl_hinfo.info_sign[0] & 2) {
        int (*bootloader_entry)(void) = (void*)IRAM1_ORIG;
        disable_irq();
        memcpy(bootloader_entry, (void*)&bl_hinfo + IM3HDR_SZ,
                    get_uint32le(bl_hinfo.data_sz));
        commit_discard_idcache();
        bootloader_entry(); /* should not return */
        status = sad;
        goto bye;
    }

#if 0
    /* check for DFU_OPT_SINGLE option */
    single_boot = bl_hinfo.info_sign[0] & 1;

    /* sign RB bootloader (data and header), but don't encrypt it,
       use current decrypted image for faster load */
    im3_sign(HWKEYAES_UKEY, (void*)&bl_hinfo + IM3HDR_SZ,
                get_uint32le(bl_hinfo.data_sz), bl_hinfo.u.enc12.data_sign);
    im3_sign(HWKEYAES_UKEY, &bl_hinfo, IM3INFOSIGN_SZ, bl_hinfo.info_sign);

    if (single_boot) {
        if (!im3_write(NORBOOT_OFF, &bl_hinfo))
            status = sad;
        goto bye;
    }

    hinfo = (struct Im3Info*)OF_LOADADDR;
    fw_addr = (void*)hinfo + IM3HDR_SZ;

    if (im3_read(NORBOOT_OFF, hinfo, fw_addr) != 0) {
        status = sad;
        goto bye; /* no FW found */
    }

    if (identify_fw(hinfo) == FW_RB) {
        /* FW found, but not OF, assume it is a RB bootloader,
           already decrypted OF should be located just behind */
        int nor_offset = NORBOOT_OFF + im3_nor_sz(hinfo);
        if ((im3_read(nor_offset, hinfo, fw_addr) != 0) ||
                                (identify_fw(hinfo) == FW_RB)) {
            status = sad;
            goto bye; /* OF not found, use iTunes to restore */
        }
    }

    bl_nor_sz = im3_nor_sz(&bl_hinfo);
    /* safety check - verify we are not going to overwrite useful data */
    if (flsh_get_unused() < bl_nor_sz) {
        status = sad2;
        goto bye; /* no space if flash, use iTunes to restore */
    }

    /* write decrypted OF and RB bootloader, if any of these fails we
       will try to retore OF to its original place */
    if (!im3_write(NORBOOT_OFF + bl_nor_sz, hinfo) ||
                            !im3_write(NORBOOT_OFF, &bl_hinfo)) {
        im3_crypt(HWKEYAES_ENCRYPT, hinfo, fw_addr);
        if (!im3_write(NORBOOT_OFF, hinfo)) {
            /* corrupted NOR, use iTunes to restore */
            status = fatal;
        }
        else {
            /* RB bootloader not succesfully intalled, but device
               was restored and should be working as before */
            status = sad;
        }
    }
#endif

bye:
    /* minimum time between the initial and the final beeps */
    while (USEC_TIMER < 2000000);
    piezo_seq(status);
    WDTCON = 0x100000; /* WDT reset */
    while (1);
}
#endif  /* DUALBOOT_UNINSTALL */
