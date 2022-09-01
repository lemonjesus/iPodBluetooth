/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Linus Nielsen Feltzing
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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "config.h"
#include "cpu.h"
#ifdef HAVE_GDB_API
#include "gdb_api.h"
#endif

#ifdef DEBUG
static char debugmembuf[200];
#if CONFIG_CPU == SH7034
static char debugbuf[400];
#endif
#endif

#include "kernel.h"
#include "system.h"
#include "debug.h"

#ifdef DEBUG
#if CONFIG_CPU == SH7034 /* these are still very SH-oriented */
void debug_init(void)
{
    /* Clear it all! */
    SSR1 &= ~(SCI_RDRF | SCI_ORER | SCI_PER | SCI_FER);

    /* This enables the serial Rx interrupt, to be able to exit into the
       debugger when you hit CTRL-C */
    SCR1 |= 0x40;
    SCR1 &= ~0x80;
    IPRE |= 0xf000; /* Set to highest priority */
}

static int debug_tx_ready(void)
{
    return (SSR1 & SCI_TDRE);
}

static void debug_tx_char(char ch)
{
    while (!debug_tx_ready())
    {
        ;
    }
    
    /*
     * Write data into TDR and clear TDRE
     */
    TDR1 = ch;
    SSR1 &= ~SCI_TDRE;
}

static void debug_handle_error(char ssr)
{
    (void)ssr;
    SSR1 &= ~(SCI_ORER | SCI_PER | SCI_FER);
}

static int debug_rx_ready(void)
{
   char ssr;
  
   ssr = SSR1 & ( SCI_PER | SCI_FER | SCI_ORER );
   if ( ssr )
      debug_handle_error ( ssr );
   return SSR1 & SCI_RDRF;
}

static char debug_rx_char(void)
{
   char ch;
   char ssr;

   while (!debug_rx_ready())
   {
      ;
   }

   ch = RDR1;
   SSR1 &= ~SCI_RDRF;

   ssr = SSR1 & (SCI_PER | SCI_FER | SCI_ORER);

   if (ssr)
      debug_handle_error (ssr);

   return ch;
}

static const char hexchars[] = "0123456789abcdef";

static char highhex(int  x)
{
   return hexchars[(x >> 4) & 0xf];
}

static char lowhex(int  x)
{
   return hexchars[x & 0xf];
}

static void putpacket (const char *buffer)
{
    register  int checksum;

    const char *src = buffer;

   /* Special debug hack. Shut off the Rx IRQ during I/O to prevent the debug
      stub from interrupting the message */
    SCR1 &= ~0x40;
   
    debug_tx_char ('$');
    checksum = 0;

    while (*src)
    {
        int runlen;
        
        /* Do run length encoding */
        for (runlen = 0; runlen < 100; runlen ++) 
        {
            if (src[0] != src[runlen] || runlen == 99) 
            {
                if (runlen > 3) 
                {
                    int encode;
                    /* Got a useful amount */
                    debug_tx_char (*src);
                    checksum += *src;
                    debug_tx_char ('*');
                    checksum += '*';
                    checksum += (encode = runlen + ' ' - 4);
                    debug_tx_char (encode);
                    src += runlen;
                }
                else
                {
                    debug_tx_char (*src);
                    checksum += *src;
                    src++;
                }
                break;
            }
        }
    }
    
    
    debug_tx_char ('#');
    debug_tx_char (highhex(checksum));
    debug_tx_char (lowhex(checksum));

    /* Wait for the '+' */
    debug_rx_char();

    /* Special debug hack. Enable the IRQ again */
    SCR1 |= 0x40;
}


/* convert the memory, pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
static char *mem2hex (const char *mem, char *buf, int count)
{
    int i;
    int ch;
    for (i = 0; i < count; i++)
    {
        ch = *mem++;
        *buf++ = highhex (ch);
        *buf++ = lowhex (ch);
    }
    *buf = 0;
    return (buf);
}

static void debug(const char *msg)
{
    debugbuf[0] = 'O';
    
    mem2hex(msg, &debugbuf[1], strlen(msg));
    putpacket(debugbuf);
}
#elif defined(HAVE_GDB_API)
static void *get_api_function(int n)
{
    struct gdb_api *api = (struct gdb_api *)GDB_API_ADDRESS;
    if (api->magic == GDB_API_MAGIC)
        return api->func[n];
    else
        return NULL;
}

void breakpoint(void)
{
    void (*f)(void) = get_api_function(0);
    if (f) (*f)();
}

static void debug(char *msg)
{
    void (*f)(char *) = get_api_function(1);
    if (f) (*f)(msg);
}

void debug_init(void)
{
}
#else /* !SH7034 && !HAVE_GDB_API */
void debug_init(void)
{
}

static inline void debug(char *msg)
{
    (void)msg;
}
#endif

#endif /* end of DEBUG section */

#ifdef __GNUC__
void debugf(const char *fmt, ...)
#endif
{
#ifdef DEBUG
    va_list ap;
    
    va_start(ap, fmt);
    vsnprintf(debugmembuf, sizeof(debugmembuf), fmt, ap);
    va_end(ap);
    debug(debugmembuf);
#else
    (void)fmt;
#endif
}

