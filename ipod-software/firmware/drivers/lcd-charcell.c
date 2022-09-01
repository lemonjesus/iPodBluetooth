/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2007 by Jens Arnold
 * Based on the work of Alan Korr, Kjell Ericson and others
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
#include "config.h"
#include "hwcompat.h"
#include "stdarg.h"
#include "lcd.h"
#include "kernel.h"
#include "thread.h"
#include "string-extra.h"
#include <stdlib.h>
#include "debug.h"
#include "file.h"
#include "system.h"
#include "lcd-charcell.h"
#include "rbunicode.h"
#include "scroll_engine.h"

/** definitions **/

#define VARIABLE_XCHARS 16 /* number of software user-definable characters */
/* There must be mappings for this many characters in the 0xe000 unicode range
 * in lcd-charset-<target>.c */

#define NO_PATTERN (-1)

static int find_xchar(unsigned long ucs);

/** globals **/

unsigned char lcd_charbuffer[LCD_HEIGHT][LCD_WIDTH];  /* The "frame"buffer */
static unsigned char lcd_substbuffer[LCD_HEIGHT][LCD_WIDTH];
struct pattern_info lcd_patterns[MAX_HW_PATTERNS];
struct cursor_info lcd_cursor;

static unsigned char xfont_variable[VARIABLE_XCHARS][HW_PATTERN_SIZE];
static bool xfont_variable_locked[VARIABLE_XCHARS];
static int xspace; /* stores xhcar id of ' ' - often needed */

static struct viewport default_vp =
  {
    .x        = 0,
    .y        = 0,
    .width    = LCD_WIDTH,
    .height   = LCD_HEIGHT,
  };

static struct viewport* current_vp = &default_vp;

/* LCD init */
void lcd_init (void)
{
    lcd_init_device();
    lcd_charset_init();
    memset(lcd_patterns, 0, sizeof(lcd_patterns));
    xspace = find_xchar(' ');
    memset(lcd_charbuffer, xchar_info[xspace].hw_char, sizeof(lcd_charbuffer));
    scroll_init();
}

/* Viewports */

void lcd_set_viewport(struct viewport* vp)
{
    if (vp == NULL)
        current_vp = &default_vp;
    else
        current_vp = vp;
        
#if defined(SIMULATOR)
    /* Force the viewport to be within bounds.  If this happens it should
     *  be considered an error - the viewport will not draw as it might be
     *  expected.
     */
    if((unsigned) current_vp->x > (unsigned) LCD_WIDTH 
        || (unsigned) current_vp->y > (unsigned) LCD_HEIGHT 
        || current_vp->x + current_vp->width > LCD_WIDTH
        || current_vp->y + current_vp->height > LCD_HEIGHT)
    {
#if !defined(HAVE_VIEWPORT_CLIP)
        DEBUGF("ERROR: "
#else
        DEBUGF("NOTE: "
#endif
            "set_viewport out of bounds: x: %d y: %d width: %d height:%d\n", 
            current_vp->x, current_vp->y, 
            current_vp->width, current_vp->height);
    }
    
#endif
}

struct viewport *lcd_get_viewport(bool *is_default)
{
    *is_default = (current_vp == &default_vp);
    return current_vp;
}

void lcd_update_viewport(void)
{
    lcd_update();
}

void lcd_update_viewport_rect(int x, int y, int width, int height)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    lcd_update();
}

/** parameter handling **/

int lcd_getwidth(void)
{
    return current_vp->width;
}

int lcd_getheight(void)
{
    return current_vp->height;
}

int lcd_getstringsize(const unsigned char *str, int *w, int *h)
{                     
    int width = utf8length(str);
    
    if (w)
        *w = width;
    if (h)
        *h = 1;
        
    return width;
}

/** low-level functions **/

static int find_xchar(unsigned long ucs)
{
    int low = 0;
    int high = xchar_info_size - 1;

    do
    {
        int probe = (low + high) >> 1;

        if (xchar_info[probe].ucs < ucs)
            low = probe + 1;
        else if (xchar_info[probe].ucs > ucs)
            high = probe - 1;
        else
            return probe;
    }
    while (low <= high);

    /* Not found: return index of no-char symbol (last symbol, hardcoded). */
    return xchar_info_size - 1;
}

static int glyph_to_pat(unsigned glyph)
{
    int i;

    for (i = 0; i < lcd_pattern_count; i++)
        if (lcd_patterns[i].glyph == glyph)
            return i;

    return NO_PATTERN;
}

static void lcd_free_pat(int pat)
{
    int x, y;

    if (pat != NO_PATTERN)
    {
        for (x = 0; x < LCD_WIDTH; x++)
            for (y = 0; y < LCD_HEIGHT; y++)
                if (pat == lcd_charbuffer[y][x])
                    lcd_charbuffer[y][x] = lcd_substbuffer[y][x];

        if (lcd_cursor.enabled && pat == lcd_cursor.hw_char)
            lcd_cursor.hw_char = lcd_cursor.subst_char;

        lcd_patterns[pat].count = 0;
    }
}

static int lcd_get_free_pat(int xchar)
{
    static int last_used_pat = 0;

    int pat = last_used_pat; /* start from last used pattern */
    int least_pat = pat;     /* pattern with least priority */
    int least_priority = lcd_patterns[pat].priority;
    int i;

    for (i = 0; i < lcd_pattern_count; i++)
    {
        if (++pat >= lcd_pattern_count)  /* Keep 'pat' within limits */
            pat = 0;
            
        if (lcd_patterns[pat].count == 0)
        {
            last_used_pat = pat;
            return pat;
        }
        if (lcd_patterns[pat].priority < least_priority)
        {
            least_priority = lcd_patterns[pat].priority;
            least_pat = pat;
        }
    }
    if (xchar_info[xchar].priority > least_priority) /* prioritized char */
    {
        lcd_free_pat(least_pat);
        last_used_pat = least_pat;
        return least_pat;
    }
    return NO_PATTERN;
}

static const unsigned char *glyph_to_pattern(unsigned glyph)
{
    if (glyph & 0x8000)
        return xfont_variable[glyph & 0x7fff];
    else
        return xfont_fixed[glyph];
}

static int map_xchar(int xchar, unsigned char *substitute)
{
    int pat;
    unsigned glyph;

    if (xchar_info[xchar].priority > 0)      /* soft char */
    {
        glyph = xchar_info[xchar].glyph;
        pat = glyph_to_pat(glyph);
 
        if (pat == NO_PATTERN)               /* not yet mapped */
        {
            pat = lcd_get_free_pat(xchar);   /* try to map */

            if (pat == NO_PATTERN)           /* failed: just use substitute */
                return xchar_info[xchar].hw_char;
            else 
            {                            /* define pattern */
                lcd_patterns[pat].priority = xchar_info[xchar].priority;
                lcd_patterns[pat].glyph = glyph;
                memcpy(lcd_patterns[pat].pattern, glyph_to_pattern(glyph),
                       HW_PATTERN_SIZE);
            }
        }
        lcd_patterns[pat].count++;           /* increase reference count */
        *substitute = xchar_info[xchar].hw_char;
        return pat;
    }
    else                                     /* hardware char */
        return xchar_info[xchar].hw_char;
}

static void lcd_putxchar(int x, int y, int xchar)
{
    int lcd_char;

    /* Adjust for viewport */
    x += current_vp->x;
    y += current_vp->y;

#if defined(HAVE_VIEWPORT_CLIP)
    if((unsigned)x > (unsigned)LCD_WIDTH || (unsigned)y > (unsigned)LCD_HEIGHT)
        return;
#endif

    lcd_char  = lcd_charbuffer[y][x];

    if (lcd_char < lcd_pattern_count)        /* old char was soft */
        lcd_patterns[lcd_char].count--;      /* decrease old reference count */

    lcd_charbuffer[y][x] = map_xchar(xchar, &lcd_substbuffer[y][x]);
}

/** user-definable pattern handling **/

unsigned long lcd_get_locked_pattern(void)
{
    int i = 0;

    for (i = 0; i < VARIABLE_XCHARS; i++)
    {
        if (!xfont_variable_locked[i])
        {
            xfont_variable_locked[i] = true;
            return 0xe000 + i;  /* hard-coded */
        }
    }
    return 0;
}

void lcd_unlock_pattern(unsigned long ucs)
{
    int xchar = find_xchar(ucs);
    unsigned glyph = xchar_info[xchar].glyph;

    if (glyph & 0x8000) /* variable extended char */
    {
        lcd_free_pat(glyph_to_pat(glyph));
        xfont_variable_locked[glyph & 0x7fff] = false;
    }
}

void lcd_define_pattern(unsigned long ucs, const char *pattern)
{
    int xchar = find_xchar(ucs);
    unsigned glyph = xchar_info[xchar].glyph;
    int pat;

    if (glyph & 0x8000) /* variable extended char */
    {
        memcpy(xfont_variable[glyph & 0x7fff], pattern, HW_PATTERN_SIZE);
        pat = glyph_to_pat(glyph);
        if (pat != NO_PATTERN)
            memcpy(lcd_patterns[pat].pattern, pattern, HW_PATTERN_SIZE);
    }
}

/** output functions **/

/* Clear the whole display */
void lcd_clear_display(void)
{
    int x, y;
    struct viewport* old_vp = current_vp;

    lcd_scroll_stop();
    lcd_remove_cursor();

    /* Set the default viewport - required for lcd_putxchar */
    current_vp = &default_vp;

    for (x = 0; x < LCD_WIDTH; x++)
        for (y = 0; y < LCD_HEIGHT; y++)
            lcd_putxchar(x, y, xspace);

    current_vp = old_vp;
}

/* Clear the current viewport */
void lcd_clear_viewport(void)
{
    int x, y;

    if (current_vp == &default_vp)
    {
        lcd_clear_display();
    }
    else
    {
        /* Remove the cursor if it is within the current viewport */
        if (lcd_cursor.enabled &&
            (lcd_cursor.x >= current_vp->x) && 
            (lcd_cursor.x <= current_vp->x + current_vp->width) &&
            (lcd_cursor.y >= current_vp->y) && 
            (lcd_cursor.y <= current_vp->y + current_vp->height))
        {
            lcd_remove_cursor();
        }

        for (x = 0; x < current_vp->width; x++)
            for (y = 0; y < current_vp->height; y++)
                lcd_putxchar(x, y, xspace);

        lcd_scroll_stop_viewport(current_vp);
    }
}

/* Put an unicode character at the given position */
void lcd_putc(int x, int y, unsigned long ucs)
{
    if ((unsigned)x >= (unsigned)current_vp->width || 
        (unsigned)y >= (unsigned)current_vp->height)
        return;

    lcd_putxchar(x, y, find_xchar(ucs));
}

/* Show cursor (alternating with existing character) at the given position */
void lcd_put_cursor(int x, int y, unsigned long cursor_ucs)
{
    if ((unsigned)x >= (unsigned)current_vp->width || 
        (unsigned)y >= (unsigned)current_vp->height ||
        lcd_cursor.enabled)
        return;

    lcd_cursor.enabled = true;
    lcd_cursor.visible = false;
    lcd_cursor.hw_char = map_xchar(find_xchar(cursor_ucs), &lcd_cursor.subst_char);
    lcd_cursor.x = current_vp->x + x;
    lcd_cursor.y = current_vp->y + y;
    lcd_cursor.downcount = 0;
    lcd_cursor.divider = MAX((HZ/2) / lcd_scroll_info.ticks, 1);
}

/* Remove the cursor */
void lcd_remove_cursor(void)
{
    if (lcd_cursor.enabled)
    {
        if (lcd_cursor.hw_char < lcd_pattern_count)  /* soft char, unmap */
            lcd_patterns[lcd_cursor.hw_char].count--;

        lcd_cursor.enabled = lcd_cursor.visible = false;
    }
}

/* Put a string at a given position, skipping first ofs chars */
static int lcd_putsxyofs(int x, int y, int ofs, const unsigned char *str)
{
    unsigned short ucs;
    const unsigned char *utf8 = str;

    while (*utf8 && x < current_vp->width)
    {
        utf8 = utf8decode(utf8, &ucs);
        
        if (ofs > 0)
        {
            ofs--;
            continue;
        }
        lcd_putxchar(x++, y, find_xchar(ucs));
    }
    return x;
}

/* Put a string at a given position */
void lcd_putsxy(int x, int y, const unsigned char *str)
{
    if ((unsigned)y >= (unsigned)current_vp->height)
        return;

    lcd_putsxyofs(x, y, 0, str);
}

/* Formatting version of lcd_putsxy */
void lcd_putsxyf(int x, int y, const unsigned char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    lcd_putsxy(x, y, buf);
}

/*** Line oriented text output ***/

/* Put a string at a given char position,  skipping first offset chars */
void lcd_putsofs(int x, int y, const unsigned char *str, int offset)
{
    if ((unsigned)y >= (unsigned)current_vp->height)
        return;

    /* make sure scrolling is turned off on the line we are updating */
    lcd_scroll_stop_viewport_rect(current_vp, x, y, current_vp->width - x, 1);

    x = lcd_putsxyofs(x, y, offset, str);
    while (x < current_vp->width)
        lcd_putxchar(x++, y, xspace);
}


/* Put a string at a given char position */
void lcd_puts(int x, int y, const unsigned char *str)
{
    lcd_putsofs(x, y, str, 0);
}

/* Formatting version of lcd_puts */
void lcd_putsf(int x, int y, const unsigned char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    lcd_puts(x, y, buf);
}

/** scrolling **/

bool lcd_puts_scroll_worker(int x, int y, const unsigned char *string,
                            int offset,
                            void (*scroll_func)(struct scrollinfo *), void *data)
{
    struct scrollinfo* s;
    int len;

    if ((unsigned)y >= (unsigned)current_vp->height)
        return false;

    /* remove any previously scrolling line at the same location */
    lcd_scroll_stop_viewport_rect(current_vp, x, y, current_vp->width - x, 1);

    if (lcd_scroll_info.lines >= LCD_SCROLLABLE_LINES)
        return false;

    s = &lcd_scroll_info.scroll[lcd_scroll_info.lines];

    s->start_tick = current_tick + lcd_scroll_info.delay;

    lcd_putsofs(x, y, string, offset);
    len = utf8length(string);

    if (current_vp->width - x >= len)
        return false;
    /* prepare scroll line */
    strlcpy(s->linebuffer, string, sizeof s->linebuffer);

    /* scroll bidirectional or forward only depending on the string width */
    if (lcd_scroll_info.bidir_limit)
    {
        s->bidir = len < (current_vp->width) *
            (100 + lcd_scroll_info.bidir_limit) / 100;
    }
    else
        s->bidir = false;

    s->scroll_func = scroll_func;
    s->userdata = data;

    s->vp = current_vp;
    s->x = x;
    s->y = y;
    s->height = 1;
    s->width = current_vp->width - x;
    s->offset = offset;
    s->backward = false;
    lcd_scroll_info.lines++;

    return true;
}

bool lcd_putsxy_scroll_func(int x, int y, const unsigned char *string,
                                     void (*scroll_func)(struct scrollinfo *),
                                     void *data, int x_offset)
{
    bool retval = false;
    if (!scroll_func)
        lcd_putsxyofs(x, y, x_offset, string);
    else
        retval = lcd_puts_scroll_worker(x, y, string, x_offset, scroll_func, data);

    return retval;
}

static void lcd_scroll_fn(struct scrollinfo* s)
{
    /* with line == NULL when scrolling stops. This scroller
     * maintains no userdata so there is nothing left to do */
    if (!s->line)
        return;
    lcd_putsxyofs(s->x, s->y, s->offset, s->line);
    if (lcd_cursor.enabled)
    {
        if (--lcd_cursor.downcount <= 0)
        {
            lcd_cursor.downcount = lcd_cursor.divider;
            lcd_cursor.visible = !lcd_cursor.visible;
        }
    }
}

bool lcd_puts_scroll(int x, int y, const unsigned char *string)
{
    return lcd_puts_scroll_worker(x, y, string, 0, lcd_scroll_fn, NULL);
}
