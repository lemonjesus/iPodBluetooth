/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:
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
#include <inttypes.h>
#include <stdbool.h>

#include "config.h"
#include "system.h"  /* udelay() */
#include "s5l8702.h"
#include "clocking-s5l8702.h"

/* returns configured frequency (PLLxFreq, when locked) */
unsigned pll_get_cfg_freq(int pll)
{
#if CONFIG_CPU == S5L8702
    unsigned pdiv, mdiv, sdiv, f_in;
    uint32_t pllpms;

    pllpms = PLLPMS(pll);

    pdiv = (pllpms >> PLLPMS_PDIV_POS) & PLLPMS_PDIV_MSK;
    if (pdiv == 0) return 0;
    mdiv = (pllpms >> PLLPMS_MDIV_POS) & PLLPMS_MDIV_MSK;
    sdiv = (pllpms >> PLLPMS_SDIV_POS) & PLLPMS_SDIV_MSK;

    /* experimental results sugests that the HW is working this way */
    if (mdiv < 2) mdiv += 256;

    if (GET_PMSMOD(pll) == PMSMOD_DIV)
    {
        f_in = (GET_DMOSC(pll))
                    ? ((PLLMOD2 & PLLMOD2_ALTOSC_BIT(pll))
                                            ? S5L8702_ALTOSC1_HZ
                                            : S5L8702_ALTOSC0_HZ)
                    : S5L8702_OSC0_HZ;

        return (f_in * mdiv / pdiv) >> sdiv; /* divide */
    }
    else
    {
        /* XXX: overflows for high f_in, safe for 32768 Hz */
        f_in = S5L8702_OSC1_HZ;
        return (f_in * mdiv * pdiv) >> sdiv; /* multiply */
    }
#else /* S5L8720 */
    // TODO
    (void) pll;
    return 0;
#endif
}

/* returns PLLxClk */
unsigned pll_get_out_freq(int pll)
{
    uint32_t pllmode = PLLMODE;

    if ((pllmode & PLLMODE_PLLOUT_BIT(pll)) == 0)
        return S5L8702_OSC1_HZ; /* slow mode */

    if ((pllmode & PLLMODE_EN_BIT(pll)) == 0)
        return 0;

    return pll_get_cfg_freq(pll);
}

/* returns selected oscillator for CG16_SEL_OSC source */
unsigned soc_get_oscsel_freq(void)
{
    return (PLLMODE & PLLMODE_OSCSEL_BIT) ? S5L8702_OSC1_HZ
                                          : S5L8702_OSC0_HZ;
}

/* returns output frequency */
unsigned cg16_get_freq(volatile uint16_t* cg16)
{
    unsigned sel, freq;
    uint16_t val16 = *cg16;

    if (val16 & CG16_DISABLE_BIT)
        return 0;

    sel = (val16 >> CG16_SEL_POS) & CG16_SEL_MSK;

    if (val16 & CG16_UNKOSC_BIT)
        freq = S5L8702_UNKOSC_HZ;
    else if (sel == CG16_SEL_OSC)
        freq = soc_get_oscsel_freq();
    else
        freq = pll_get_out_freq(--sel);

    freq /= (((val16 >> CG16_DIV1_POS) & CG16_DIV1_MSK) + 1) *
                (((val16 >> CG16_DIV2_POS) & CG16_DIV2_MSK) + 1);

    return freq;
}

void soc_set_system_divs(unsigned cdiv, unsigned hdiv, unsigned hprat)
{
#if CONFIG_CPU == S5L8702
    uint32_t val = 0;
    unsigned pdiv = hdiv * hprat;

    if (cdiv > 1)
        val |= CLKCON1_CDIV_EN_BIT |
                ((((cdiv >> 1) - 1) & CLKCON1_CDIV_MSK) << CLKCON1_CDIV_POS);
    if (hdiv > 1)
        val |= CLKCON1_HDIV_EN_BIT |
                ((((hdiv >> 1) - 1) & CLKCON1_HDIV_MSK) << CLKCON1_HDIV_POS);
    if (pdiv > 1)
        val |= CLKCON1_PDIV_EN_BIT |
                ((((pdiv >> 1) - 1) & CLKCON1_PDIV_MSK) << CLKCON1_PDIV_POS);

// TODO: XXX: el 8720 pone aqui el EN_BIT, no se usa en s5l8702, ver si influye, parece que se NO comporta igual, y cuando es todo 0 -> 0x40 no va (TBC), ver EFI
    val |= ((hprat - 1) & CLKCON1_HPRAT_MSK) << CLKCON1_HPRAT_POS;

    CLKCON1 = val;

    while ((CLKCON1 >> 8) != (val >> 8));
#else
    // TODO
    (void) cdiv;
    (void) hdiv;
    (void) hprat;
#endif
}

unsigned soc_get_system_divs(unsigned *cdiv, unsigned *hdiv, unsigned *pdiv)
{
    uint32_t val = CLKCON1;

    if (cdiv)
        *cdiv = !(val & CLKCON1_CDIV_EN_BIT) ? 1 :
                (((val >> CLKCON1_CDIV_POS) & CLKCON1_CDIV_MSK) + 1) << 1;
    if (hdiv)
        *hdiv = !(val & CLKCON1_HDIV_EN_BIT) ? 1 :
                (((val >> CLKCON1_HDIV_POS) & CLKCON1_HDIV_MSK) + 1) << 1;
    if (pdiv)
        *pdiv = !(val & CLKCON1_PDIV_EN_BIT) ? 1 :
                (((val >> CLKCON1_PDIV_POS) & CLKCON1_PDIV_MSK) + 1) << 1;

    return cg16_get_freq(&CG16_SYS);  /* FClk */
}

unsigned get_system_freqs(unsigned *cclk, unsigned *hclk, unsigned *pclk)
{
    unsigned fclk, cdiv, hdiv, pdiv;

    fclk = soc_get_system_divs(&cdiv, &hdiv, &pdiv);

    if (cclk) *cclk = fclk / cdiv;
    if (hclk) *hclk = fclk / hdiv;
    if (pclk) *pclk = fclk / pdiv;

    return fclk;
}

void soc_set_hsdiv(int hsdiv)
{
    SM1_DIV = hsdiv - 1;
}

int soc_get_hsdiv(void)
{
    return (SM1_DIV & 0xf) + 1;
}

/* each target/app could define its own clk_modes table */
struct clocking_mode *clk_modes;
int cur_level = -1;

void clocking_init(struct clocking_mode *modes, int level)
{
    /* at this point, CK16_SYS should be already configured
       and enabled by emCORE/bootloader */

    /* initialize global clocking */
    clk_modes = modes;
    cur_level = level;

    /* start initial level */
    struct clocking_mode *m = clk_modes + cur_level;
    soc_set_hsdiv(m->hsdiv);
    soc_set_system_divs(m->cdiv, m->hdiv, m->hprat);
}

void set_clocking_level(int level)
{
    struct clocking_mode *cur, *next;

    int step = (level < cur_level) ? -1 : 1;

    while (cur_level != level)
    {
        cur = clk_modes + cur_level;
        next = cur + step;

        /* step-up */
        if (next->hsdiv > cur->hsdiv)
            soc_set_hsdiv(next->hsdiv);

        /* step up/down */
        soc_set_system_divs(next->cdiv, next->hdiv, next->hprat);

        /* step-down */
        if (next->hsdiv < cur->hsdiv)
            soc_set_hsdiv(next->hsdiv);

        cur_level += step;
    }

    udelay(50); /* TBC: probably not needed */
}

void clockgate_enable(int gate, bool enable)
{
    //int i = (gate >> 5) & 1;
    int i = gate >> 5;
    uint32_t bit = 1 << (gate & 0x1f);
    if (enable) PWRCON(i) &= ~bit;
    else PWRCON(i) |= bit;
}

int soc_get_sec_epoch(void)
{
#if CONFIG_CPU == S5L8720
    // XXX
    // TBC: sec_epoch == 0 -> OSC0 = 24MHz (TBC) -> o igual es 12
    //                        internal SDRAM (TBC) -> o igual es un multiplicador de la SDRAM ???
    //                   1 -> OSC0 = 48MHz (TBC) -> o igual es 24
    //                        external SDRAM (TBC) -> o igual es un multiplicador de la SDRAM ???
    // segun ROMBOOT: si bit2 de sec_epoch == 1 -> usa /CN=Apple Secure Boot Certification Auth... (ver bootrom_main)
    //                                        0 ->     /CN=Apple IPod Certification Authority...
    clockgate_enable(CLOCKGATE_CHIPID, true);
    int sec_epoch = CHIPID_INFO & 1;
    clockgate_enable(CLOCKGATE_CHIPID, false);
    return sec_epoch;
#else /* S5L8702 */
    #if 0
    GPIOCMD = 0x30500;  /* configure GPIO3.5 as input */
    return !!(PDAT3 & 0x20);
    #else
    return 0;   // XXX: en classics/nano3g parece que siempre es 0, ademas PDAT3 se usa por LCD(TBC) y por eso no podemos
                //      andar mirandolo en RB, solo al inicio del BL
                // XXX: tambien podemos intentar leerlo solo en el preinit y guardarlo para luego ???
    #endif
#endif
}


#ifdef BOOTLOADER
int pll_config(int pll, int op_mode, int p, int m, int s, int lock_time)
{
    PLLPMS(pll) = ((p & PLLPMS_PDIV_MSK) << PLLPMS_PDIV_POS)
                | ((m & PLLPMS_MDIV_MSK) << PLLPMS_MDIV_POS)
                | ((s & PLLPMS_SDIV_MSK) << PLLPMS_SDIV_POS);

    /* lock_time are PClk ticks */
    PLLCNT(pll) = lock_time & PLLCNT_MSK;

#if CONFIG_CPU == S5L8702
    if (pll < 2)
    {
        if (op_mode == PLLOP_MM) {
            PLLMODE &= ~PLLMODE_PMSMOD_BIT(pll);  /* MM */
            return 0;
        }

        PLLMODE |= PLLMODE_PMSMOD_BIT(pll);

        if (op_mode == PLLOP_DM) {
            PLLMOD2 &= ~PLLMOD2_DMOSC_BIT(pll);   /* DM */
            return 0;
        }

        PLLMOD2 |= PLLMOD2_DMOSC_BIT(pll);
    }
    else
    {
        if (op_mode == PLLOP_MM)
            return -1;  /* PLL2 does not support MM */

        if (op_mode == PLLOP_DM) {
            PLLMODE &= ~PLLMODE_PLL2DMOSC_BIT;    /* DM */
            return 0;
        }

        PLLMODE |= PLLMODE_PLL2DMOSC_BIT;
    }

    /* ALTOSCx */
    PLLMOD2 = (PLLMOD2 & ~PLLMOD2_ALTOSC_BIT(pll)) |
                ((op_mode == PLLOP_ALT0) ? 0 : PLLMOD2_ALTOSC_BIT(pll));

#else /* S5L8720 */
    // XXX: en el 8702, PLL0,1 no se comportan igual que en 8702, parece que
    //      se podrian comportar de forma similar a PLL2 en 8702 (TODO: comprobar,
    //      posiblemente podrian no tener modo MM), ver el tema de ALTOSC, igual
    //      el PLLUNK3C hace una funcion similar en 8720 a PLLMOD2 en 8702
    if (op_mode == PLLOP_DM) {
        PLLMODE &= ~PLLMODE_PMSMOD_BIT(pll);
        return 0;
    }
    return -1;
//     // TBC:
//     if (op_mode == PLLOP_MM)
//         return -1;  /* PLLx does not support MM */
//     PLLMODE |= PLLMODE_PMSMOD_BIT(pll);
//     PLLUNK3C = ???
#endif

    return 0;
}

int pll_onoff(int pll, bool onoff)
{
    if (onoff)
    {
        PLLMODE |= PLLMODE_EN_BIT(pll); /* start PLL */
        while (!(PLLLOCK & PLLLOCK_LCK_BIT(pll))); /* locking... */
        PLLMODE |= PLLMODE_PLLOUT_BIT(pll); /* slow mode OFF */

        /* returns DMLCK status, only meaningful for Divisor Mode (DM) */
        return (PLLLOCK & PLLLOCK_DMLCK_BIT(pll)) ? 1 : 0;
    }
    else
    {
        PLLMODE &= ~PLLMODE_PLLOUT_BIT(pll); /* slow mode ON */
        udelay(50); /* TBC: needed when current F_in is 0 Hz */
        PLLMODE &= ~PLLMODE_EN_BIT(pll); /* stop PLL */

        return 0;
    }
}

/* configure and enable/disable 16-bit clockgate */
#if (CONFIG_CPU == S5L8720)
void cg16_config(volatile uint16_t* cg16,
                    bool onoff, int clksel, int div1, int div2, int flags)
#else
void cg16_config(volatile uint16_t* cg16,
                    bool onoff, int clksel, int div1, int div2)
#endif
{
    uint16_t val16 = ((clksel & CG16_SEL_MSK) << CG16_SEL_POS)
                   | (((div1 - 1) & CG16_DIV1_MSK) << CG16_DIV1_POS)
                   | (((div2 - 1) & CG16_DIV2_MSK) << CG16_DIV2_POS)
#if (CONFIG_CPU == S5L8720)
                   | flags
#endif
                   | (onoff ? 0 : CG16_DISABLE_BIT);

    volatile uint32_t* reg32 = (uint32_t *)((int)cg16 & ~3);
    int shift = ((int)cg16 & 2) << 3;

    *reg32 = (*reg32 & (0xffff0000 >> shift)) | (val16 << shift);

    /*udelay(100);*/  /* probably not needed */

    while (*cg16 != val16);
}

/* Configures EClk for USEC_TIMER. DRAM refresh also depends on EClk,
 * this clock should be initialized by the bootloader, so USEC_TIMER
 * is ready to use for RB.
 */
// TODO: creo que no se resetea usec timer a 0
void usec_timer_init(void)
{
    /* select OSC0 for CG16 SEL_OSC */
    PLLMODE &= ~PLLMODE_OSCSEL_BIT;

    /* configure and enable ECLK */
#if CONFIG_CPU == S5L8720
    cg16_config(&CG16_RTIME, true, CG16_SEL_OSC, 1, 1, 0x0);

    /* unmask timer controller clock gates */
//     clockgate_enable(CLOCKGATE_TIMERA, true);            // TODO: igual estas no hacen falta
    clockgate_enable(CLOCKGATE_TIMERE, true);
//     clockgate_enable(CLOCKGATE_TIMERA_2, true);
    clockgate_enable(CLOCKGATE_TIMERE_2, true);
#else /* S5L8702 */
    cg16_config(&CG16_RTIME, true, CG16_SEL_OSC, 1, 1);

    /* unmask timer controller clock gate */
    clockgate_enable(CLOCKGATE_TIMER, true);
#endif

    /* configure and start timer E */
    TECON = (4 << 8) |      /* TE_CS = ECLK / 1 */
            (1 << 6) |      /* select ECLK (12 MHz on Classic) */
            (0 << 4);       /* TE_MODE_SEL = interval mode */
    TEPRE = (S5L8702_OSC0_HZ / 1000000) - 1;  /* prescaler */
    TEDATA0 = ~0;
    TECMD = (1 << 1) |      /* TE_CLR = initialize timer */
            (1 << 0);       /* TE_EN = enable */
}
#endif  /* BOOTLOADER */
