/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2008 by Mark Arigo
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

#include <stdlib.h>
#include "config.h"
#include "cpu.h"
#include "system.h"
#include "kernel.h"
#include "button-target.h"
#include "synaptics-mep.h"

/*#define LOGF_ENABLE*/
#include "logf.h"

/* Driver for the Synaptics Touchpad based on the "Synaptics Modular Embedded
   Protocol: 3-Wire Interface Specification" documentation */

#if defined(MROBE_100)
#define INT_ENABLE  GPIO_CLEAR_BITWISE(GPIOD_INT_LEV, 0x2);\
                    GPIO_SET_BITWISE(GPIOD_INT_EN, 0x2)
#define INT_DISABLE GPIO_CLEAR_BITWISE(GPIOD_INT_EN, 0x2);\
                    GPIO_SET_BITWISE(GPIOD_INT_CLR, 0x2)

#define ACK     (GPIOD_INPUT_VAL & 0x1)
#define ACK_HI  GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x1)
#define ACK_LO  GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x1)

#define CLK     ((GPIOD_INPUT_VAL & 0x2) >> 1)
#define CLK_HI  GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x2)
#define CLK_LO  GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x2)

#define DATA    ((GPIOD_INPUT_VAL & 0x4) >> 2)
#define DATA_HI GPIO_SET_BITWISE(GPIOD_OUTPUT_EN, 0x4);\
                GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x4)
#define DATA_LO GPIO_SET_BITWISE(GPIOD_OUTPUT_EN, 0x4);\
                GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x4)
#define DATA_CL GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_EN, 0x4)

#elif defined(PHILIPS_HDD1630) || defined(PHILIPS_HDD6330) || \
      defined(PBELL_VIBE500)
#define INT_ENABLE  GPIO_CLEAR_BITWISE(GPIOA_INT_LEV, 0x20);\
                    GPIO_SET_BITWISE(GPIOA_INT_EN, 0x20)
#define INT_DISABLE GPIO_CLEAR_BITWISE(GPIOA_INT_EN, 0x20);\
                    GPIO_SET_BITWISE(GPIOA_INT_CLR, 0x20)

#define ACK     (GPIOD_INPUT_VAL & 0x80)
#define ACK_HI  GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x80)
#define ACK_LO  GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x80)

#define CLK     ((GPIOA_INPUT_VAL & 0x20) >> 5)
#define CLK_HI  GPIO_SET_BITWISE(GPIOA_OUTPUT_VAL, 0x20)
#define CLK_LO  GPIO_CLEAR_BITWISE(GPIOA_OUTPUT_VAL, 0x20)

#define DATA    ((GPIOA_INPUT_VAL & 0x10) >> 4)
#define DATA_HI GPIO_SET_BITWISE(GPIOA_OUTPUT_EN, 0x10);\
                GPIO_SET_BITWISE(GPIOA_OUTPUT_VAL, 0x10)
#define DATA_LO GPIO_SET_BITWISE(GPIOA_OUTPUT_EN, 0x10);\
                GPIO_CLEAR_BITWISE(GPIOA_OUTPUT_VAL, 0x10)
#define DATA_CL GPIO_CLEAR_BITWISE(GPIOA_OUTPUT_EN, 0x10)

#elif defined(PHILIPS_SA9200)
#define INT_ENABLE  GPIO_CLEAR_BITWISE(GPIOD_INT_LEV, 0x2);\
                    GPIO_SET_BITWISE(GPIOD_INT_EN, 0x2)
#define INT_DISABLE GPIO_CLEAR_BITWISE(GPIOD_INT_EN, 0x2);\
                    GPIO_SET_BITWISE(GPIOD_INT_CLR, 0x2)

#define ACK     (GPIOD_INPUT_VAL & 0x8)
#define ACK_HI  GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x8)
#define ACK_LO  GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x8)

#define CLK     ((GPIOD_INPUT_VAL & 0x2) >> 1)
#define CLK_HI  GPIO_SET_BITWISE(GPIOD_OUTPUT_VAL, 0x2)
#define CLK_LO  GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x2)

#define DATA    ((GPIOD_INPUT_VAL & 0x10) >> 4)
#define DATA_HI GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_EN, 0x10)
#define DATA_LO GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_VAL, 0x10);\
                GPIO_SET_BITWISE(GPIOD_OUTPUT_EN, 0x10)
#define DATA_CL GPIO_CLEAR_BITWISE(GPIOD_OUTPUT_EN, 0x10)
#endif

#define LO 0
#define HI 1

#define READ_RETRY      8
#define READ_ERROR     -1

#define MEP_HELLO_HEADER 0x19
#define MEP_HELLO_ID     0x1

#define MEP_READ  0x1
#define MEP_WRITE 0x3

static unsigned short syn_status = 0;

static void syn_enable_int(bool enable)
{
    if (enable)
    {
        INT_ENABLE;
    }
    else
    {
        INT_DISABLE;
    }
}

static int syn_wait_clk_change(unsigned int val)
{
    int i;

    for (i = 0; i < 10000; i++)
    {
        if (CLK == val)
            return 1;
    }

    return 0;
}

static void syn_set_ack(int val)
{
    if (val == HI)
    {
        ACK_HI;
    }
    else
    {
        ACK_LO;
    }
}

static void syn_set_data(int val)
{
    if (val == HI)
    {
        DATA_HI;
    }
    else
    {
        DATA_LO;
    }
}

static inline int syn_get_data(void)
{
    DATA_CL;
#if defined(PBELL_VIBE500) /* for EABI (touchpad doesn't work without it) */
    udelay(0);
#endif
    return DATA;
}

static void syn_wait_guest_flush(void)
{
    /* Flush receiving (flushee) state:
       handshake until DATA goes high during P3 stage */
    if (CLK == LO)
    {
        syn_set_ack(HI);         /* P1 -> P2 */
        syn_wait_clk_change(HI); /* P2 -> P3 */
    }

    while (syn_get_data() == LO)
    {
        syn_set_ack(HI);         /* P3 -> P0 */
        syn_wait_clk_change(LO); /* P0 -> P1 */
        syn_set_ack(LO);         /* P1 -> P2 */
        syn_wait_clk_change(HI); /* P2 -> P3 */
    }

    /* Continue handshaking until back to P0 */
    syn_set_ack(HI);             /* P3 -> P0 */
}

static void syn_flush(void)
{
    int i;

    logf("syn_flush...");

    /* Flusher holds DATA low for at least 36 handshake cycles */
    syn_set_data(LO);

    for (i = 0; i < 36; i++)
    {
        syn_wait_clk_change(LO); /* P0 -> P1 */
        syn_set_ack(LO);         /* P1 -> P2 */
        syn_wait_clk_change(HI); /* P2 -> P3 */
        syn_set_ack(HI);         /* P3 -> P0 */
    }

    /* Raise DATA in P1 stage */
    syn_wait_clk_change(LO); /* P0 -> P1 */
    syn_set_data(HI);

    /* After a flush, the flushing device enters a flush-receiving (flushee)
       state */
    syn_wait_guest_flush();
}

static int syn_send(char *data, int len)
{
    int i, bit;
    int parity = 0;

    logf("syn_send...");

    /* 1. Lower DATA line to issue a request-to-send to guest */
    syn_set_data(LO);

    /* 2. Wait for guest to lower CLK */
    syn_wait_clk_change(LO);

    /* 3. Lower ACK (with DATA still low) */
    syn_set_ack(LO);

    /* 4. Wait for guest to raise CLK */
    syn_wait_clk_change(HI);

    /* 5. Send data */
    for (i = 0; i < len; i++)
    {
       logf("  sending byte: %d", data[i]);

        bit = 0;
        while (bit < 8)
        {
            /* 5a. Drive data low if bit is 0, or high if bit is 1 */
            if (data[i] & (1 << bit))
            {
                syn_set_data(HI);
                parity++;
            }
            else
            {
                syn_set_data(LO);
            }
            bit++;

            /* 5b. Invert ACK to indicate that the data bit is ready */
            syn_set_ack(HI);

            /* 5c. Wait for guest to invert CLK */
            syn_wait_clk_change(LO);

            /* Repeat for next bit */
            if (data[i] & (1 << bit))
            {
                syn_set_data(HI);
                parity++;
            }
            else
            {
                syn_set_data(LO);
            }
            bit++;

            syn_set_ack(LO);

            syn_wait_clk_change(HI);
        }
    }

    /* 7. Transmission termination sequence: */
    /* 7a. Host may put parity bit on DATA. Hosts that do not generate
           parity should set DATA high. Parity is 1 if there's an odd
           number of '1' bits, or 0 if there's an even number of '1' bits. */
    parity = parity % 2;
    if (parity)
    {
        syn_set_data(HI);
    }
    else
    {
        syn_set_data(LO);
    }
    logf("  send parity = %d", parity);

    /* 7b. Raise ACK to indicate that the optional parity bit is ready */
    syn_set_ack(HI);

    /* 7c. Guest lowers CLK */
    syn_wait_clk_change(LO);

    /* 7d. Pull DATA high (if parity bit was 0) */
    syn_set_data(HI);

    /* 7e. Lower ACK to indicate that the stop bit is ready */
    syn_set_ack(LO);

    /* 7f. Guest raises CLK */
    syn_wait_clk_change(HI);

    /* 7g. If DATA is low, guest is flushing this transfer. Host should
           enter the flushee state. */
    if (syn_get_data() == LO)
    {
        logf("  module flushing");

        syn_wait_guest_flush();
        return -1;
    }

    /* 7h. Host raises ACK and the link enters the idle state */
    syn_set_ack(HI);

    return len;
}

static int syn_read_data(char *data, int data_len)
{
    int i, len, bit, parity;
    char *data_ptr, tmp;

    logf("syn_read_data...");

    /* 1. Guest drives CLK low */
    if (CLK != LO)
        return 0;

    /* 1a. If the host is willing to receive a packet it lowers ACK */
    syn_set_ack(LO);

    /* 2. Guest may issue a request-to-send by lowering DATA. If the
          guest decides not to transmit a packet, it may abort the
          transmission by not lowering DATA. */

    /* 3. The guest raises CLK */
    syn_wait_clk_change(HI);

    /* 4. If the guest is still driving DATA low, the transfer is commited
          to occur. Otherwise, the transfer is aborted. In either case, 
          the host raises ACK. */
    if (syn_get_data() == HI)
    {
        logf("  read abort");

        syn_set_ack(HI);
        return READ_ERROR;
    }
    else
    {
        syn_set_ack(HI);
    }

    /* 5. Read the incoming data packet */
    i = 0;
    len = 0;
    parity = 0;
    while (i <= len)
    {
        bit = 0;

        if (i < data_len)
            data_ptr = &data[i];
        else
            data_ptr = &tmp;

        *data_ptr = 0;
        while (bit < 8)
        {
            /* 5b. Guset inverts CLK to indicate that data is ready */
            syn_wait_clk_change(LO);

            /* 5d. Read the data bit from DATA */
            if (syn_get_data() == HI)
            {
                *data_ptr |= (1 << bit);
                parity++;
            }
            bit++;

            /* 5e. Invert ACK to indicate that data has been read */
            syn_set_ack(LO);

            /* Repeat for next bit */
            syn_wait_clk_change(HI);

            if (syn_get_data() == HI)
            {
                *data_ptr |= (1 << bit);
                parity++;
            }
            bit++;

            syn_set_ack(HI);
        }

        /* First byte is the packet header */
        if (i == 0)
        {
            /* Format control (bit 3) should be 1 */
            if (*data_ptr & 0x8)
            {
                /* Packet length is bits 0:2 */
                len = *data_ptr & 0x7;
                logf("  packet length = %d", len);
            }
            else
            {
                logf("  invalid format ctrl bit");
                return READ_ERROR;
            }
        }

        i++;
    }

    /* 7. Transmission termination cycle */
    /* 7a. The guest generates a parity bit on DATA */
    /* 7b. The host waits for guest to lower CLK */
    syn_wait_clk_change(LO);

    /* 7c. The host verifies the parity bit is correct */
    parity = parity % 2;
    logf("  parity check: %d / %d", syn_get_data(), parity);

    /* TODO: parity error handling */

    /* 7d. The host lowers ACK */
    syn_set_ack(LO);

    /* 7e. The host waits for the guest to raise CLK indicating 
           that the stop bit is ready */
    syn_wait_clk_change(HI);

    /* 7f. The host reads DATA and verifies that it is 1 */
    if (syn_get_data() == LO)
    {
        logf("  framing error");

        syn_set_ack(HI);
        return READ_ERROR;
    }

    syn_set_ack(HI);

    return len;
}

static int syn_read(char *data, int len)
{
    int i;
    int ret = READ_ERROR;

    for (i = 0; i < READ_RETRY; i++)
    {
        if (syn_wait_clk_change(LO))
        {
            /* module is sending data */
            ret = syn_read_data(data, len);
            if (ret != READ_ERROR)
                return ret;

            syn_flush();
        }
        else
        {
            /* module is idle */
            return 0;
        }
    }

    return ret;
}

static int syn_reset(void)
{
    int val, id;
    char data[2];

    logf("syn_reset...");

    /* reset module 0 */
    data[0] = (0 << 4) | (1 << 3) | 0;
    syn_send(data, 1);

    val = syn_read(data, 2);
    if (val == 1)
    {
        val = data[0] & 0xff;      /* packet header */
        id = (data[1] >> 4) & 0xf; /* packet id */
        if ((val == MEP_HELLO_HEADER) && (id == MEP_HELLO_ID))
        {
            logf("  module 0 reset");
            return 1;
        }
    }

    logf("  reset failed");
    return 0;
}

int touchpad_init(void)
{
    syn_flush();
    syn_status = syn_reset();

    if (syn_status)
    {
        /* reset interrupts */
        syn_enable_int(false);
        syn_enable_int(true);

        CPU_INT_EN    |= HI_MASK;
        CPU_HI_INT_EN |= GPIO0_MASK;
    }

    return syn_status;
}

int touchpad_read_device(char *data, int len)
{
    char tmp[4];
    int id;
    int val = 0;

    if (syn_status)
    {
        /* disable interrupt while we read the touchpad */
        syn_enable_int(false);

        val = syn_read(data, len);
        if (val > 0)
        {
            val = data[0] & 0xff;      /* packet header */
            id = (data[1] >> 4) & 0xf; /* packet id */

            logf("syn_read:");
            logf("  data[0] = 0x%08x", data[0]);
            logf("  data[1] = 0x%08x", data[1]);
            logf("  data[2] = 0x%08x", data[2]);
            logf("  data[3] = 0x%08x", data[3]);

            if ((val == MEP_BUTTON_HEADER) && (id == MEP_BUTTON_ID))
            {
                /* an absolute packet should follow which we ignore */
                syn_read(tmp, 4);
            }
            else if (val == MEP_ABSOLUTE_HEADER)
            {
/* for HDD6330 an absolute packet will follow for sensor nr 0 which we ignore */
#if defined(PHILIPS_HDD6330)
                if ((data[3]>>6) == 0) syn_read(tmp, 4);
                // relay tap gesture packet
                if (tmp[1]==0x02) { data[1]=0x02; data[2]=0x00; data[3]=0x00; }
#endif
                logf(" pos %d", val);
                logf(" z %d", data[3]);
                logf(" finger %d", data[1] & 0x1);
                logf(" gesture %d", data[1] & 0x2);
                logf(" RelPosVld %d", data[1] & 0x4);

                if (!(data[1] & 0x1))
                {
                    /* finger is NOT on touch strip */
                    val = 0;
                }
            }
            else
            {
                val = 0;
            }
        }

        /* re-enable interrupts */
        syn_enable_int(true);
    }

    return val;
}

int touchpad_set_parameter(char mod_nr, char par_nr, unsigned int param)
{
    char data[4];
    int i, val=0;

    if (syn_status)
    {
        syn_enable_int(false);

        data[0]=0x03 | (mod_nr << 5); /* header - addr=mod_nr,global:0,ctrl:0,len:3 */
        data[1]=0x40+par_nr; /* parameter number */
        data[2]=(param >> 8) & 0xff; /* param_hi */
        data[3]=param & 0xff;        /* param_lo */
        syn_send(data,4);
        val=syn_read(data,4); /* try to get the simple ACK = 0x18 */

        /* modules > 0 sometimes don't give ACK immediately but other packets like */
        /* absolute from the scroll strip, so it has to be ignored until we receive ACK */
        if ((mod_nr > 0) && ((data[0] & 7) != 0))
        for (i = 0; i < 2; i++)
        {
            val=syn_read(data,4);
            if (data[0] == 0x18) break;
        }

        syn_enable_int(true);
    }
    return val;
}

#if 0
/* Not used normally, but useful for pulling settings or determining
   which parameters are supported */
int touchpad_get_parameter(char mod_nr, char par_nr, unsigned int *param_p)
{
    char data[4];
    int val = 0;

    if (syn_status)
    {
        syn_enable_int(false);

        /* 'Get MEP Parameter' command packet */
        data[0]=0x01 | (mod_nr << 5); /* header - addr=mod_nr,global:0,ctrl:0,len:1 */
        data[1]=0x40+par_nr; /* parameter number */
        syn_send(data,2);

        /* Must not be an error packet; check size */
        if (syn_read(data,4) == 3)
        {
            /* ACK: param_hi[15:8], param_lo[7:0] */
            if (param_p)
                *param_p = ((unsigned int)data[2] << 8) | data[3];
            val = 3;
        }

        syn_enable_int(true);
    }

    return val;
}
#endif

int touchpad_set_buttonlights(unsigned int led_mask, char brightness)
{
    char data[6];
    int val = 0;

    if (syn_status)
    {
        syn_enable_int(false);

        /* turn on all touchpad leds */
#if defined(PHILIPS_HDD6330)
        data[0] = 0x25; /* HDD6330: second module */
#else
        data[0] = 0x05;
#endif
        data[1] = 0x31;
        data[2] = (brightness & 0xf) << 4;
        data[3] = 0x00;
        data[4] = (led_mask >> 8) & 0xff;
        data[5] = led_mask & 0xff;
        syn_send(data, 6);

        /* device responds with a single-byte ACK packet */
        val = syn_read(data, 2);

        syn_enable_int(true);
    }

    return val;
}

#ifdef ROCKBOX_HAS_LOGF
void syn_info(void)
{
    int i, val;
    int data[8];

    logf("syn_info...");

    /* module base info */
    logf("module base info:");
    data[0] = MEP_READ;
    data[1] = 0x80;
    syn_send(data, 2);
    val = syn_read(data, 8);
    if (val > 0)
    {
        for (i = 0; i < 8; i++)
            logf("  data[%d] = 0x%02x", i, data[i]);
    }

    /* module product info */
    logf("module product info:");
    data[0] = MEP_READ;
    data[1] = 0x81;
    syn_send(data, 2);
    val = syn_read(data, 8);
    if (val > 0)
    {
        for (i = 0; i < 8; i++)
            logf("  data[%d] = 0x%02x", i, data[i]);
    }

    /* module serialization */
    logf("module serialization:");
    data[0] = MEP_READ;
    data[1] = 0x82;
    syn_send(data, 2);
    val = syn_read(data, 8);
    if (val > 0)
    {
        for (i = 0; i < 8; i++)
            logf("  data[%d] = 0x%02x", i, data[i]);
    }

    /* 1-D sensor info */
    logf("1-d sensor info:");
    data[0] = MEP_READ;
    data[1] = 0x80 + 0x20;
    syn_send(data, 2);
    val = syn_read(data, 8);
    if (val > 0)
    {
        for (i = 0; i < 8; i++)
            logf("  data[%d] = 0x%02x", i, data[i]);
    }
}
#endif
