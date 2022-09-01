/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2015 by Cástor Muñoz
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "mks5lboot.h"

/* Header for ARM code binaries */
#include "dualboot-8702.h"
#include "dualboot-8720.h"

/* Win32 compatibility */
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * This code is based on emCORE, adds a couple of improvements due to some
 * extra features in Apple's ROM:
 * - Generic Im3Info header valid for all payloads. It is done by moving
 *   the certificate to a fixed position (before data), and usin 'magic'
 *   values of 0x300 for signature offset and 0x0 for data size.
 *   (Really for the boot ROM, any value <= 0x300 disables de data lenght check)
 * - Some integer overflows in ROM allows to put the certificate at the
 *   free space available in Im3Hdr, resulting a maximum payload size of:
 *   128 KiB - 0x50 bytes (IM3INFO_SZ) - 700 bytes (CERT_SIZE).
 */


#if 0
const struct ipod_models ipod_identity[] =
{
    [MODEL_IPOD6G] = {
        "iPod Classic 6G", "ipod6g", "ip6g", 71,
        dualboot_install_ipod6g,   sizeof(dualboot_install_ipod6g),
        dualboot_uninstall_ipod6g, sizeof(dualboot_uninstall_ipod6g) },
    [MODEL_IPODNANO3G] = {
        "iPod Nano 3G", "ipodnano3g", "nn3g", 96,
        dualboot_install_ipodnano3g,   sizeof(dualboot_install_ipodnano3g),
        dualboot_uninstall_ipodnano3g, sizeof(dualboot_uninstall_ipodnano3g) },
#if 0
    [MODEL_IPODNANO4G] = {
        "iPod Nano 4G", "ipodnano4g", "nn4g", 97,
        dualboot_install_ipodnano4g,   sizeof(dualboot_install_ipodnano4g),
        dualboot_uninstall_ipodnano4g, sizeof(dualboot_uninstall_ipodnano4g) },
#endif
};
#endif


/* s5l8702 */
struct Im3Info s5l8702hdr =
{
    .ident          = IM3_IDENT,
    .version        = IM3_VERSION,
    .enc_type       = 3,
    .u.enc34 = {
        .sign_off   = "\x00\x03\x00\x00",
        .cert_off   = "\x50\xF8\xFF\xFF", /* CERT_OFFSET - 0x800 */
        .cert_sz    = "\xBA\x02\x00\x00", /* CERT_SIZE */
    },
    .info_sign      = "\xC2\x54\x51\x31\xDC\xC0\x84\xA4"
                      "\x7F\xD1\x45\x08\xE8\xFF\xE8\x1D",
};

#if 1
unsigned char s5l8702pwnage[CERT_SIZE] =
#else
unsigned char s5l8702pwnage[/*CERT_SIZE*/] =
#endif
{
    "\x30\x82\x00\x7A\x30\x66\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86"
    "\xF7\x0D\x01\x01\x05\x05\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55"
    "\x04\x03\x13\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03\x13"
    "\x00\x30\x19\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01"
    "\x05\x00\x03\x08\x00\x30\x05\x02\x01\x00\x02\x00\x30\x0D\x06\x09"
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x01\x00\x30\x82"
    "\x00\x7A\x30\x66\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D"
    "\x01\x01\x05\x05\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03"
    "\x13\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03\x13\x00\x30"
    "\x19\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01\x05\x00"
    "\x03\x08\x00\x30\x05\x02\x01\x00\x02\x00\x30\x0D\x06\x09\x2A\x86"
    "\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x01\x00\x30\x82\x01\xBA"
    "\x30\x50\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01"
    "\x05\x05\x00\x30\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x30\x00\x30\x19\x30\x0D\x06\x09\x2A\x86\x48"
    "\x86\xF7\x0D\x01\x01\x01\x05\x00\x03\x08\x00\x30\x05\x02\x01\x00"
    "\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05"
    "\x00\x03\x82\x01\x55"
};

// TODO
#if 1
/* s5l8720 */
struct Im3Info s5l8720hdr =
{
    .ident          = IM320_IDENT,
    .version        = IM320_VERSION,
    .enc_type       = 3,
    .u.enc34 = {
        .sign_off   = "\x00\x03\x00\x00",
        .cert_off   = "\x50\xFA\xFF\xFF", /* CERT_OFFSET - 0x600 */
        .cert_sz    = "\xBE\x02\x00\x00", /* CERT20_SIZE */
    },
    // TODO
    // 5E A0 98 D2 │ 2A 43 30 41 │ 1F 98 0A 85 │ 02 13 ED 1C
    .info_sign      = "\x5E\xA0\x98\xD2\x2A\x43\x30\x41"
                      "\x1F\x98\x0A\x85\x02\x13\xED\x1C",
};

unsigned char s5l8720pwnage[CERT20_SIZE] =
{
    "\x30\x82\x00\x7A\x30\x66\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86"
    "\xF7\x0D\x01\x01\x05\x05\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55"
    "\x04\x03\x13\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03\x13"
    "\x00\x30\x19\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01"
    "\x05\x00\x03\x08\x00\x30\x05\x02\x01\x00\x02\x00\x30\x0D\x06\x09"
    "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x01\x00\x30\x82"
    "\x00\x7A\x30\x66\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D"
    "\x01\x01\x05\x05\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03"
    "\x13\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x30\x0B\x31\x09\x30\x07\x06\x03\x55\x04\x03\x13\x00\x30"
    "\x19\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01\x05\x00"
    "\x03\x08\x00\x30\x05\x02\x01\x00\x02\x00\x30\x0D\x06\x09\x2A\x86"
//     "\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x01\x00\x30\x82\x01\xBA"
    "\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x01\x00\x30\x82\x01\xBE"
    "\x30\x50\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01"
    "\x05\x05\x00\x30\x00\x30\x1E\x17\x0D\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x17\x0D\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x30\x00\x30\x19\x30\x0D\x06\x09\x2A\x86\x48"
    "\x86\xF7\x0D\x01\x01\x01\x05\x00\x03\x08\x00\x30\x05\x02\x01\x00"
    "\x02\x00\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05"
//     "\x00\x03\x82\x01\x55"
    "\x00\x03\x82\x01\x59"
};


/* models */
const struct soc_type soc_list[] =
{
    [SOC_S5L8702] = { "s5l8702", &s5l8702hdr, s5l8702pwnage, IM3HDR_SZ },
    [SOC_S5L8720] = { "s5l8720", &s5l8720hdr, s5l8720pwnage, IM320HDR_SZ },
};

const struct ipod_models ipod_identity[] =
{
    [MODEL_IPOD6G] = {
        "iPod Classic 6G", "ipod6g", "ip6g", 71,
        dualboot_install_ipod6g,   sizeof(dualboot_install_ipod6g),
        dualboot_uninstall_ipod6g, sizeof(dualboot_uninstall_ipod6g),
        &soc_list[SOC_S5L8702] },
    [MODEL_IPODNANO3G] = {
        "iPod Nano 3G", "ipodnano3g", "nn3g", 96,
        dualboot_install_ipodnano3g,   sizeof(dualboot_install_ipodnano3g),
        dualboot_uninstall_ipodnano3g, sizeof(dualboot_uninstall_ipodnano3g),
        &soc_list[SOC_S5L8702] },
    [MODEL_IPODNANO4G] = {
        "iPod Nano 4G", "ipodnano4g", "nn4g", 97,
        dualboot_install_ipodnano4g,   sizeof(dualboot_install_ipodnano4g),
        dualboot_uninstall_ipodnano4g, sizeof(dualboot_uninstall_ipodnano4g),
        &soc_list[SOC_S5L8720] },
};
#endif

static uint32_t get_uint32le(unsigned char* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static uint32_t get_uint32be(unsigned char* p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static void put_uint32le(unsigned char* p, uint32_t x)
{
    p[0] = x & 0xff;
    p[1] = (x >> 8) & 0xff;
    p[2] = (x >> 16) & 0xff;
    p[3] = (x >> 24) & 0xff;
}

#define _ERR(format, ...) \
    do { \
        snprintf(errstr, errstrsize, "[ERR] "format, __VA_ARGS__); \
        goto error; \
    } while(0)

static unsigned char *load_file(char *filename, int *bufsize,
                                const struct ipod_models** model,
                                char* errstr, int errstrsize)
{
    int fd, i;
    struct stat s;
    unsigned char header[8];
    unsigned char *buf = NULL;
    bool is_rbbl = (model);

    fd = open(filename, O_RDONLY|O_BINARY);
    if (fd < 0)
        _ERR("Could not open %s for reading", filename);

    if (fstat(fd, &s) < 0)
        _ERR("Checking filesize of input file %s", filename);
    *bufsize = s.st_size;

    if (is_rbbl) {
        /* Read Rockbox header */
        if (read(fd, header, sizeof(header)) != sizeof(header))
            _ERR("Could not read file %s", filename);
        *bufsize -= sizeof(header);

        for (i = 0; i < NUM_MODELS; i++)
            if (memcmp(ipod_identity[i].rb_model_name, header + 4, 4) == 0)
                break;

        if (i == NUM_MODELS)
            _ERR("Model name \"%4.4s\" unknown. "
                    "Is this really a rockbox bootloader?", header + 4);

        *model = &ipod_identity[i];
    }

    buf = malloc(*bufsize);
    if (buf == NULL)
        _ERR("Could not allocate memory for %s", filename);

    if (read(fd, buf, *bufsize) != *bufsize)
        _ERR("Could not read file %s", filename);

    if (is_rbbl) {
        /* Check checksum */
        uint32_t sum = (*model)->rb_model_num;
        for (i = 0; i < *bufsize; i++) {
             /* add 8 unsigned bits but keep a 32 bit sum */
             sum += buf[i];
        }
        if (sum != get_uint32be(header))
            _ERR("Checksum mismatch in %s", filename);
    }

    close(fd);
    return buf;

error:
    if (fd >= 0)
        close(fd);
    if (buf)
        free(buf);
    return NULL;
}

#if 0
/* Supported SOCs */
enum {
    SOC_S5L8702 = 0,
    SOC_S5L8720,
    NUM_SOCS
};

struct soc_models {
    char* name;
    struct Im3Info *s5l_hdr;
    unsigned char *s5l_pwnage;
    int im3hdr_sz;
};

const struct soc_type soc_l[] =
{
    [SOC_S5L8702] = { "s5l8702", &s5l8702hdr, s5l8702pwnage, IM3HDR_SZ },
    [SOC_S5L8720] = { "s5l8720", &s5l8720hdr, s5l8720pwnage, IM320HDR_SZ },
};
#endif


unsigned char *mkdfu(int dfu_type, char *dfu_arg, int* dfu_size,
                                        char* errstr, int errstrsize)
{
    const struct ipod_models *model = NULL;
    unsigned char *dfu_buf = NULL;
    unsigned char *f_buf;
    int f_size;
    uint32_t padded_bl_size;
    uint32_t cert_off, cert_sz;
    off_t cur_off;
    char *dfu_desc;
    int i;

    int dfu_opt = dfu_type >> DFU_OPTIONS_POS;
    dfu_type &= (1 << DFU_OPTIONS_POS) - 1;

#if 1
    const struct soc_type *soc;
//     const struct soc_models *soc = &soc_list[
//             (dfu_opt & DFU_OPT_S5L8720) ? SOC_S5L8720: SOC_S5L8702];
#else
    int im3hdr_sz;
    struct Im3Info *s5l_hdr;
    unsigned char *s5l_pwnage;
    if (dfu_opt & DFU_OPT_S5L8720) {
        im3hdr_sz   = IM320HDR_SZ;            // TODO IM320HDR_SZ -> IM3HDR20_SZ, IM3HDR02_SZ ???
        s5l_hdr     = &s5l8720hdr;
        s5l_pwnage  = s5l8720pwnage;
    }
    else {
        im3hdr_sz   = IM3HDR_SZ;
        s5l_hdr     = &s5l8702hdr;
        s5l_pwnage  = s5l8702pwnage;
    }
#endif

    switch (dfu_type)
    {
        case DFU_INST:
//         case DFU_INST_SINGLE:
            f_buf = load_file(dfu_arg, &f_size, &model, errstr, errstrsize);
            if (f_buf == NULL)
                goto error;
#if 1
            soc = model->soc;
#endif
            /* IM3 data size should be padded to 16 */
            padded_bl_size = ((f_size + 0xf) & ~0xf);
            *dfu_size = BIN_OFFSET + (model->dualboot_install_size +
#if 1
//                         (im3hdr_sz - (int)IM3INFO_SZ) + (int)padded_bl_size);
                        (soc->im3hdr_sz - (int)IM3INFO_SZ) + (int)padded_bl_size);
#else
                        (IM3HDR_SZ - (int)IM3INFO_SZ) + (int)padded_bl_size);
#endif
//             dfu_desc = (dfu_options & DFU_OPT_SINGLE) ? "BL installer (single)"
//                                                       : "BL installer";
            dfu_desc = (dfu_opt & DFU_OPT_NONPERSISTENT)
                        ? "BL installer (non-persistent)"
                        : ((dfu_opt & DFU_OPT_SINGLE) ? "BL intaller (single)"
                                                      : "BL installer");
            break;

        case DFU_UNINST:
            for (i = 0; i < NUM_MODELS; i++) {
                if (!strcmp(ipod_identity[i].platform_name, dfu_arg)) {
                    model = &ipod_identity[i];
                    break;
                }
            }
            if (!model)
                _ERR("Platform name \"%s\" unknown", dfu_arg);

#if 1
            soc = model->soc;
#endif
            *dfu_size = BIN_OFFSET + model->dualboot_uninstall_size;
            dfu_desc = "BL uninstaller";
            break;

        case DFU_RAW:
        default:
            f_buf = load_file(dfu_arg, &f_size, NULL, errstr, errstrsize);
            if (f_buf == NULL)
                goto error;
            /* IM3 data size should be padded to 16 */
            *dfu_size = BIN_OFFSET + ((f_size + 0xf) & ~0xf);
#if 1
//             dfu_desc = (dfu_opt & DFU_OPT_S5L8720) ?
//                         "raw binary (s5l8720)" : "raw binary (s5l8702)";
            dfu_desc = "raw binary";
            soc = &soc_list[(dfu_opt & DFU_OPT_S5L8720) ? SOC_S5L8720 : SOC_S5L8702];
#else
            dfu_desc = "raw binary";
#endif
            break;
    }

#if 1
    printf("[INFO] Building %s DFU:\n", soc->name);
#else
    printf("[INFO] Building DFU:\n");
#endif
    printf("[INFO]  type:         %s\n", dfu_desc);
//     if ((dfu_type == DFU_INST) || (dfu_type == DFU_INST_SINGLE))
    if (dfu_type == DFU_INST)
        printf("[INFO]  BL size:      %d\n", f_size);
    if (dfu_type == DFU_RAW)
        printf("[INFO]  BIN size:     %d\n", f_size);
    printf("[INFO]  DFU size:     %d\n", *dfu_size);
    if (model) {
        printf("[INFO]  model name:   %s\n", model->model_name);
        printf("[INFO]  platform:     %s\n", model->platform_name);
        printf("[INFO]  RB name:      %s\n", model->rb_model_name);
        printf("[INFO]  RB num:       %d\n", model->rb_model_num);
    }

    if (*dfu_size > DFU_MAXSIZE)
        _ERR("DFU image (%d bytes) too big", *dfu_size);

    dfu_buf = calloc(*dfu_size, 1);
    if (!dfu_buf)
        _ERR("Could not allocate %d bytes for DFU image", *dfu_size);

#if 1
    cert_off = get_uint32le(soc->s5l_hdr->u.enc34.cert_off);
    cert_sz  = get_uint32le(soc->s5l_hdr->u.enc34.cert_sz);

    memcpy(dfu_buf, soc->s5l_hdr, sizeof(struct Im3Info));

    cur_off = soc->im3hdr_sz + cert_off;

    memcpy(dfu_buf + cur_off, soc->s5l_pwnage, cert_sz);

#elif 1
    cert_off = get_uint32le(s5l_hdr->u.enc34.cert_off);
    cert_sz  = get_uint32le(s5l_hdr->u.enc34.cert_sz);

    memcpy(dfu_buf, s5l_hdr, sizeof(struct Im3Info));

    cur_off = im3hdr_sz + cert_off;

    memcpy(dfu_buf + cur_off, s5l_pwnage, cert_sz);
#else
    cert_off = get_uint32le(s5l8702hdr.u.enc34.cert_off);
    cert_sz  = get_uint32le(s5l8702hdr.u.enc34.cert_sz);

    memcpy(dfu_buf, &s5l8702hdr, sizeof(s5l8702hdr));

    cur_off = IM3HDR_SZ + cert_off;

    memcpy(dfu_buf + cur_off, s5l8702pwnage, sizeof(s5l8702pwnage));
#endif

    /* set entry point */
    cur_off += cert_sz - 4;
    put_uint32le(dfu_buf + cur_off, DFU_LOADADDR + BIN_OFFSET);

    cur_off = BIN_OFFSET;

    switch (dfu_type)
    {
        case DFU_INST:
//         case DFU_INST_SINGLE:
            /* copy the dualboot installer binary */
            memcpy(dfu_buf + cur_off, model->dualboot_install,
                                        model->dualboot_install_size);
            /* point to the start of the included IM3 header info */
            cur_off += model->dualboot_install_size - IM3INFO_SZ;
            /* set bootloader data size */
            struct Im3Info *bl_info = (struct Im3Info*)(dfu_buf + cur_off);
            put_uint32le(bl_info->data_sz, padded_bl_size);
//             if (dfu_type == DFU_INST_SINGLE)
//                 bl_info->info_sign[0] = 0x1;
            /* use info_sign to pass options to the dualboot installer */
            bl_info->info_sign[0] = dfu_opt;
            /* add bootloader binary */
#if 1
            cur_off += soc->im3hdr_sz;
//             cur_off += im3hdr_sz;
#else
            cur_off += IM3HDR_SZ;
#endif
            memcpy(dfu_buf + cur_off, f_buf, f_size);
            break;

        case DFU_UNINST:
            /* copy the dualboot uninstaller binary */
            memcpy(dfu_buf + cur_off, model->dualboot_uninstall,
                                        model->dualboot_uninstall_size);
            break;

        case DFU_RAW:
        default:
            /* add raw binary */
            memcpy(dfu_buf + cur_off, f_buf, f_size);
            break;
    }

    return dfu_buf;

error:
    if (dfu_buf)
        free(dfu_buf);
    return NULL;
}
