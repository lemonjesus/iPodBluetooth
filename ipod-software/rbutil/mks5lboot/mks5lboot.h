/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:$
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
#ifndef __MK6GBOOT_H__
#define __MK6GBOOT_H__

/* useful for dualboot.lds */
#define DFU_LOADADDR    0x22000000
#define DFU_MAXSIZE     0x20000  /* maximum .dfu file size */

#define CERT_OFFSET     0x50  /* IM3INFO_SZ */
#define CERT_SIZE       698
// XXX: utilizamos el cert size mayor para calcular el BIN_OFFSET y no complicarnos por 4 bytes de diferencia
#define CERT20_SIZE     702
#define BIN_OFFSET      (CERT_OFFSET + ((CERT20_SIZE + 0x3) & ~ 0x3))
// #define BIN_OFFSET      (CERT_OFFSET + ((CERT_SIZE + 0x3) & ~ 0x3))
#define MAX_PAYLOAD     (DFU_MAXSIZE - BIN_OFFSET)



#ifndef ASM
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IM3_IDENT       "8702"
#define IM3_VERSION     "1.0"
#define IM3HDR_SZ       0x800

#define IM320_IDENT     "8720"
#define IM320_VERSION   "2.0"
#define IM320HDR_SZ     0x600

#define IM3INFO_SZ      (sizeof(struct Im3Info))
#define IM3INFOSIGN_SZ  (offsetof(struct Im3Info, info_sign))   /* to sign size */

#define SIGN_SZ     16  /* signature size */

struct Im3Info
{
    uint8_t ident[4];
    uint8_t version[3];
    uint8_t enc_type;
    uint8_t entry[4];   /* LE */
    uint8_t data_sz[4]; /* LE */
    union {
        struct {
            uint8_t data_sign[SIGN_SZ];
            uint8_t _reserved[32];
        } enc12;
        struct {
            uint8_t sign_off[4]; /* LE */
            uint8_t cert_off[4]; /* LE */
            uint8_t cert_sz[4];  /* LE */
            uint8_t _reserved[36];
        } enc34;
    } u;
    uint8_t info_sign[SIGN_SZ];
} __attribute__ ((packed));

struct Im3Hdr
{
    struct Im3Info info;
    uint8_t _zero[IM3HDR_SZ - sizeof(struct Im3Info)];
} __attribute__ ((packed));


#if 1
/* Supported SOCs */
enum {
    SOC_S5L8702 = 0,
    SOC_S5L8720,
    /* new SOCs go here */
    NUM_SOCS
};

struct soc_type {
    char* name;
    struct Im3Info *s5l_hdr;
    unsigned char *s5l_pwnage;
    int im3hdr_sz;
};
#endif

/* Supported models */
enum {
    MODEL_UNKNOWN = -1,
    MODEL_IPOD6G = 0,
    MODEL_IPODNANO3G,
    MODEL_IPODNANO4G,
    /* new models go here */
    NUM_MODELS
};

struct ipod_models {
    /* Descriptive name of this model */
    const char* model_name;
    /* For bootloader uninstallers */
    const char* platform_name;
    /* Model name used in the Rockbox header in ".ipod" files - these match the
       -add parameter to the "scramble" tool */
    const char* rb_model_name;
    /* Model number used to initialise the checksum in the Rockbox header in
       ".ipod" files - these are the same as MODEL_NUMBER in config-target.h */
    const int rb_model_num;
    /* Dualboot functions for this model */
    const unsigned char* dualboot_install;
    int dualboot_install_size;
    const unsigned char* dualboot_uninstall;
    int dualboot_uninstall_size;
#if 1
    /* SoC used by this model */
    const struct soc_type* soc;
#endif
};
extern const struct ipod_models ipod_identity[];


enum {
    DFU_NONE = -1,
    DFU_INST = 0,       /* RB installer */
//     DFU_INST_SINGLE,    /* RB installer (single) */
    DFU_UNINST,         /* RB uninstaller */
    DFU_RAW             /* raw binary */
};

#define DFU_OPT_SINGLE          (1 << 0)
#define DFU_OPT_NONPERSISTENT   (1 << 1)
#if 1
#define DFU_OPT_S5L8720         (1 << 2)
#endif
#define DFU_OPTIONS_POS     8


unsigned char *mkdfu(int dfu_type, char *dfu_arg, int* dfu_size,
                                char* errstr, int errstrsize);

int ipoddfu_send(int pid, unsigned char *buf, int size,
                                char* errstr, int errstrsize);
int ipoddfu_scan(int pid, int *state, int reset,
                                char* errstr, int errstrsize);
void ipoddfu_debug(int debug);

#ifdef __cplusplus
};
#endif
#endif /* ASM */

#endif /* __MK6GBOOT_H__ */
