/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 Dave Chapman
 *     Text rendering
 * Copyright (C) 2006 Shachar Liberman
 *     Offset text, scrolling
 * Copyright (C) 2007 Nicolas Pennequin, Tom Ross, Ken Fazzone, Akio Idehara
 *     Color gradient background
 * Copyright (C) 2009 Andrew Mahone
 *     Merged common LCD bitmap code
 *
 * Rockbox common bitmap LCD functions
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
#include <stdarg.h>
#include <stdio.h>
#include "string-extra.h"
#include "diacritic.h"

#ifndef LCDFN /* Not compiling for remote - define macros for main LCD. */
#define LCDFN(fn) lcd_ ## fn
#define FBFN(fn)  fb_ ## fn
#define LCDM(ma) LCD_ ## ma
#define LCDNAME "lcd_"
#define MAIN_LCD
#endif

void LCDFN(set_framebuffer)(FBFN(data) *fb)
{
    if (fb)
        LCDFN(framebuffer) = fb;
    else
        LCDFN(framebuffer) = &LCDFN(static_framebuffer)[0][0];
}

/*
 * draws the borders of the current viewport
 **/
void LCDFN(draw_border_viewport)(void)
{
    LCDFN(drawrect)(0, 0, current_vp->width, current_vp->height);
}

/*
 * fills the rectangle formed by current_vp
 **/
void LCDFN(fill_viewport)(void)
{
    LCDFN(fillrect)(0, 0, current_vp->width, current_vp->height);
}


/*** Viewports ***/

void LCDFN(set_viewport)(struct viewport* vp)
{
    if (vp == NULL)
        current_vp = &default_vp;
    else
        current_vp = vp;

#if LCDM(DEPTH) > 1
    LCDFN(set_foreground)(current_vp->fg_pattern);
    LCDFN(set_background)(current_vp->bg_pattern);
#endif

#if defined(SIMULATOR)
    /* Force the viewport to be within bounds.  If this happens it should
     *  be considered an error - the viewport will not draw as it might be
     *  expected.
     */
    if((unsigned) current_vp->x > (unsigned) LCDM(WIDTH)
        || (unsigned) current_vp->y > (unsigned) LCDM(HEIGHT)
        || current_vp->x + current_vp->width > LCDM(WIDTH)
        || current_vp->y + current_vp->height > LCDM(HEIGHT))
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

struct viewport *LCDFN(get_viewport)(bool *is_default)
{
    *is_default = (current_vp == &default_vp);
    return current_vp;
}

void LCDFN(update_viewport)(void)
{
    LCDFN(update_rect)(current_vp->x, current_vp->y,
                    current_vp->width, current_vp->height);
}

void LCDFN(update_viewport_rect)(int x, int y, int width, int height)
{
    LCDFN(update_rect)(current_vp->x + x, current_vp->y + y, width, height);
}

/* put a string at a given pixel position, skipping first ofs pixel columns */
static void LCDFN(putsxyofs)(int x, int y, int ofs, const unsigned char *str)
{
    unsigned short *ucs;
    font_lock(current_vp->font, true);
    struct font* pf = font_get(current_vp->font);
    int vp_flags = current_vp->flags;
    int rtl_next_non_diac_width, last_non_diacritic_width;

    if ((vp_flags & VP_FLAG_ALIGNMENT_MASK) != 0)
    {
        int w;

        LCDFN(getstringsize)(str, &w, NULL);
        /* center takes precedence */
        if (vp_flags & VP_FLAG_ALIGN_CENTER)
        {
            x = ((current_vp->width - w)/ 2) + x;
            if (x < 0)
                x = 0;
        }
        else
        {
            x = current_vp->width - w - x;
            x += ofs;
            ofs = 0;
        }
    }

    rtl_next_non_diac_width = 0;
    last_non_diacritic_width = 0;
    /* Mark diacritic and rtl flags for each character */
    for (ucs = bidi_l2v(str, 1); *ucs; ucs++)
    {
        bool is_rtl, is_diac;
        const unsigned char *bits;
        int width, base_width, drawmode = 0, base_ofs = 0;
        const unsigned short next_ch = ucs[1];

        if (x >= current_vp->width)
            break;

        is_diac = is_diacritic(*ucs, &is_rtl);

        /* Get proportional width and glyph bits */
        width = font_get_width(pf, *ucs);

        /* Calculate base width */
        if (is_rtl)
        {
            /* Forward-seek the next non-diacritic character for base width */
            if (is_diac)
            {
                if (!rtl_next_non_diac_width)
                {
                    const unsigned short *u;

                    /* Jump to next non-diacritic char, and calc its width */
                    for (u = &ucs[1]; *u && is_diacritic(*u, NULL); u++);

                    rtl_next_non_diac_width = *u ?  font_get_width(pf, *u) : 0;
                }
                base_width = rtl_next_non_diac_width;
            }
            else
            {
                rtl_next_non_diac_width = 0; /* Mark */
                base_width = width;
            }
        }
        else
        {
            if (!is_diac)
                last_non_diacritic_width = width;

            base_width = last_non_diacritic_width;
        }

        if (ofs > width)
        {
            ofs -= width;
            continue;
        }

        if (is_diac)
        {
            /* XXX: Suggested by amiconn:
             * This will produce completely wrong results if the original
             * drawmode is DRMODE_COMPLEMENT. We need to pre-render the current
             * character with all its diacritics at least (in mono) and then
             * finally draw that. And we'll need an extra buffer that can hold
             * one char's bitmap. Basically we can't just change the draw mode
             * to something else irrespective of the original mode and expect
             * the result to look as intended and with DRMODE_COMPLEMENT (which
             * means XORing pixels), overdrawing this way will cause odd results
             * if the diacritics and the base char both have common pixels set.
             * So we need to combine the char and its diacritics in a temp
             * buffer using OR, and then draw the final bitmap instead of the
             * chars, without touching the drawmode
             **/
            drawmode = current_vp->drawmode;
            current_vp->drawmode = DRMODE_FG;

            base_ofs = (base_width - width) / 2;
        }

        bits = font_get_bits(pf, *ucs);

#if defined(MAIN_LCD) && defined(HAVE_LCD_COLOR)
        if (pf->depth)
            lcd_alpha_bitmap_part(bits, ofs, 0, width, x + base_ofs, y,
                                  width - ofs, pf->height);
        else
#endif
            LCDFN(mono_bitmap_part)(bits, ofs, 0, width, x + base_ofs,
                                    y, width - ofs, pf->height);
        if (is_diac)
        {
            current_vp->drawmode = drawmode;
        }

        if (next_ch)
        {
            bool next_is_rtl;
            bool next_is_diacritic = is_diacritic(next_ch, &next_is_rtl);

            /* Increment if:
             *  LTR: Next char is not diacritic,
             *  RTL: Current char is non-diacritic and next char is diacritic */
            if ((is_rtl && !is_diac) ||
                    (!is_rtl && (!next_is_diacritic || next_is_rtl)))
            {
                x += base_width - ofs;
                ofs = 0;
            }
        }
    }
    font_lock(current_vp->font, false);
}

/*** pixel oriented text output ***/

/* put a string at a given pixel position */
void LCDFN(putsxy)(int x, int y, const unsigned char *str)
{
    LCDFN(putsxyofs)(x, y, 0, str);
}

/* Formatting version of LCDFN(putsxy) */
void LCDFN(putsxyf)(int x, int y, const unsigned char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    LCDFN(putsxy)(x, y, buf);
}

/*** Line oriented text output ***/

void LCDFN(puts)(int x, int y, const unsigned char *str)
{
    int h;
    x *= LCDFN(getstringsize)(" ", NULL, &h);
    y *= h;
    LCDFN(putsxyofs)(x, y, 0, str);
}

/* Formatting version of LCDFN(puts) */
void LCDFN(putsf)(int x, int y, const unsigned char *fmt, ...)
{
    va_list ap;
    char buf[256];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof (buf), fmt, ap);
    va_end(ap);
    LCDFN(puts)(x, y, buf);
}

/*** scrolling ***/

static struct scrollinfo* find_scrolling_line(int x, int y)
{
    struct scrollinfo* s = NULL;
    int i;

    for(i=0; i<LCDFN(scroll_info).lines; i++)
    {
        s = &LCDFN(scroll_info).scroll[i];
        if (s->x == x && s->y == y && s->vp == current_vp)
            return s;
    }
    return NULL;
}

void LCDFN(scroll_fn)(struct scrollinfo* s)
{
    /* with line == NULL when scrolling stops. This scroller
     * maintains no userdata so there is nothing left to do */
    if (!s->line)
        return;
    /* Fill with background/backdrop to clear area.
     * cannot use clear_viewport_rect() since would stop scrolling as well */
    LCDFN(set_drawmode)(DRMODE_SOLID|DRMODE_INVERSEVID);
    LCDFN(fillrect)(s->x, s->y, s->width, s->height);
    LCDFN(set_drawmode)(DRMODE_SOLID);
    LCDFN(putsxyofs)(s->x, s->y, s->offset, s->line);
}

static bool LCDFN(puts_scroll_worker)(int x, int y, const unsigned char *string,
                                     int x_offset,
                                     bool linebased,
                                     void (*scroll_func)(struct scrollinfo *),
                                     void *data)
{
    struct scrollinfo* s;
    int width, height;
    int w, h, cwidth;
    bool restart;

    if (!string)
        return false;

    /* prepare rectangle for scrolling. x and y must be calculated early
     * for find_scrolling_line() to work */
    cwidth = font_get(current_vp->font)->maxwidth;
    height = font_get(current_vp->font)->height;
    y = y * (linebased ? height : 1);
    x = x * (linebased ? cwidth : 1);
    width = current_vp->width - x;

    if (y >= current_vp->height)
        return false;

    s = find_scrolling_line(x, y);
    restart = !s;

    /* get width (pixeks) of the string */
    LCDFN(getstringsize)(string, &w, &h);

    /* Remove any previously scrolling line at the same location. If
     * the string width is too small to scroll the scrolling line is
     * cleared as well */
    if (w < width || restart) {
        LCDFN(scroll_stop_viewport_rect)(current_vp, x, y, width, height);
        LCDFN(putsxyofs)(x, y, x_offset, string);
        /* nothing to scroll, or out of scrolling lines. Either way, get out */
        if (w < width || LCDFN(scroll_info).lines >= LCDM(SCROLLABLE_LINES))
            return false;
        /* else restarting: prepare scroll line */
        s = &LCDFN(scroll_info).scroll[LCDFN(scroll_info).lines];
    }

    /* copy contents to the line buffer */
    strlcpy(s->linebuffer, string, sizeof(s->linebuffer));
    /* scroll bidirectional or forward only depending on the string width */
    if ( LCDFN(scroll_info).bidir_limit ) {
        s->bidir = w < (current_vp->width) *
            (100 + LCDFN(scroll_info).bidir_limit) / 100;
    }
    else
        s->bidir = false;

    if (restart) {
        s->offset = x_offset;
        s->backward = false;
        /* assign the rectangle. not necessary if continuing an earlier line */
        s->x = x;
        s->y = y;
        s->width = width;
        s->height = height;
        s->vp = current_vp;
        s->start_tick = current_tick + LCDFN(scroll_info).delay;
        LCDFN(scroll_info).lines++;
    } else {
        /* not restarting, however we are about to assign new userdata;
         * therefore tell the scroller that it can release the previous userdata */
        s->line = NULL;
        s->scroll_func(s);
    }

    s->scroll_func = scroll_func;
    s->userdata = data;

    /* if only the text was updated render immediately */
    if (!restart)
        LCDFN(scroll_now(s));

    return true;
}

bool LCDFN(putsxy_scroll_func)(int x, int y, const unsigned char *string,
                                     void (*scroll_func)(struct scrollinfo *),
                                     void *data, int x_offset)
{
    bool retval = false;
    if (!scroll_func)
        LCDFN(putsxyofs)(x, y, x_offset, string);
    else
        retval = LCDFN(puts_scroll_worker)(x, y, string, x_offset, false, scroll_func, data);

    return retval;
}

bool LCDFN(puts_scroll)(int x, int y, const unsigned char *string)
{
    return LCDFN(puts_scroll_worker)(x, y, string, 0, true, LCDFN(scroll_fn), NULL);
}

#if !defined(HAVE_LCD_COLOR) || !defined(MAIN_LCD)
/* see lcd-16bit-common.c for others */
#ifdef MAIN_LCD
#define THIS_STRIDE STRIDE_MAIN
#else
#define THIS_STRIDE STRIDE_REMOTE
#endif

void LCDFN(bmp_part)(const struct bitmap* bm, int src_x, int src_y,
                                int x, int y, int width, int height)
{
#if LCDM(DEPTH) > 1
    if (bm->format != FORMAT_MONO)
        LCDFN(bitmap_part)((FBFN(data)*)(bm->data),
            src_x, src_y, THIS_STRIDE(bm->width, bm->height), x, y, width, height);
    else
#endif
        LCDFN(mono_bitmap_part)(bm->data,
            src_x, src_y, THIS_STRIDE(bm->width, bm->height), x, y, width, height);
}

void LCDFN(bmp)(const struct bitmap* bm, int x, int y)
{
    LCDFN(bmp_part)(bm, 0, 0, x, y, bm->width, bm->height);
}

#endif

void LCDFN(nine_segment_bmp)(const struct bitmap* bm, int x, int y,
                                int width, int height)
{
    int seg_w, seg_h, src_x, src_y, dst_x, dst_y;
    /* if the bitmap dimensions are not multiples of 3 bytes reduce the
     * inner segments accordingly. A 8x8 image becomes 3x3 for each
     * corner, and 2x2 for the inner segments */
    int corner_w = (bm->width + 2) / 3;
    int corner_h = (bm->height + 2) / 3;
    seg_w = bm->width  - (2 * corner_w);
    seg_h = bm->height - (2 * corner_h);

    /* top & bottom in a single loop*/
    src_x = corner_w;
    dst_x = corner_w;
    int src_y_top = 0;
    int dst_y_top = 0;
    int src_y_btm = bm->height - corner_h;
    int dst_y_btm = height - corner_h;
    for (; dst_x < width - seg_w; dst_x += seg_w)
    {
        /* cap the last segment to the remaining width */
        int w = MIN(seg_w, (width - dst_x - seg_w));
        LCDFN(bmp_part)(bm, src_x, src_y_top, dst_x, dst_y_top, w, seg_h);
        LCDFN(bmp_part)(bm, src_x, src_y_btm, dst_x, dst_y_btm, w, seg_h);
    }
        
    /* left & right in a single loop */
    src_y = corner_h;
    dst_y = corner_h;
    int src_x_l = 0;
    int dst_x_l = 0;
    int src_x_r = bm->width - corner_w;
    int dst_x_r = width - corner_w;
    for (; dst_y < height - seg_h; dst_y += seg_h)
    {
        /* cap the last segment to the remaining height */
        int h = MIN(seg_h, (height - dst_y - seg_h));
        LCDFN(bmp_part)(bm, src_x_l, src_y, dst_x_l, dst_y, seg_w, h);
        LCDFN(bmp_part)(bm, src_x_r, src_y, dst_x_r, dst_y, seg_w, h);
    }
    /* center, need not be drawn if the desired rectangle is smaller than
     * the sides. in that case the rect is completely filled already */
    if (width > (2*corner_w) && height > (2*corner_h))
    {
        dst_y = src_y = corner_h;
        src_x = corner_w;
        for (; dst_y < height - seg_h; dst_y += seg_h)
        {
            /* cap the last segment to the remaining height */
            int h = MIN(seg_h, (height - dst_y - seg_h));
            dst_x = corner_w;
            for (; dst_x < width - seg_w; dst_x += seg_w)
            {
                /* cap the last segment to the remaining width */
                int w = MIN(seg_w, (width - dst_x - seg_w));
                LCDFN(bmp_part)(bm, src_x, src_y, dst_x, dst_y, w, h);
            }
        }
    }
    /* 4 corners */
    LCDFN(bmp_part)(bm, 0, 0, x, y, corner_w, corner_h);
    LCDFN(bmp_part)(bm, bm->width - corner_w, 0, width - corner_w, 0, corner_w, corner_h);
    LCDFN(bmp_part)(bm, 0, bm->width - corner_h, 0, height - corner_h, corner_w, corner_h);
    LCDFN(bmp_part)(bm, bm->width - corner_w, bm->width - corner_h,
            width - corner_w, height - corner_h, corner_w, corner_h);
}
