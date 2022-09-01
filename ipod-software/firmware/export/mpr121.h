/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2012 Amaury Pouly
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
#ifndef __MPR121_H__
#define __MPR121_H__

/** Registers for the Freescale MPR121 Capacitive Proximity Sensor */
#include "system.h"

/* Touch status: EL{0,7} */
#define REG_TOUCH_STATUS    0x00
#define REG_TOUCH_STATUS__ELE(x)    (1 << (x))
/* Touch status: EL{8-11,prox}, overcurrent */
#define REG_TOUCH_STATUS2   0x01
#define REG_TOUCH_STATUS2__ELE(x)   (1 << ((x) - 8))
#define REG_TOUCH_STATUS2__ELEPROX  (1 << 4)
#define REG_TOUCH_STATUS2__OVCF     (1 << 7)
/* Out of range: EL{0,7} */
#define REG_OOR_STATUS      0x02
#define REG_OOR_STATUS__ELE(x)      (1 << (x))
/* Out of range: EL{8-11,prox}, autoconf err */
#define REG_OOR_STATUS2     0x03
#define REG_OOR_STATUS2__ELE(x)     (1 << (x))
#define REG_OOR_STATUS2__ELEPROX    (1 << 4)
#define REG_OOR_STATUS2__ACFF       (1 << 6)
#define REG_OOR_STATUS2__ARFF       (1 << 7)
/* Electrode X filtered data LSB */
#define REG_EFDxLB(x)       (0x04 + 0x02 * (x))
/* Electrode X filtered data MSB */
#define REG_EFDxHB(x)       (0x05 + 0x02 * (x))
/* Proximity electrode X filtered data LSB */
#define REG_EFDPROXLB       0x1c
/* Proximity electrode X filtered data MSB */
#define REG_EFDPROXHB       0x1d
/* Electrode baseline value */
#define REG_ExBV(x)         (0x1e + (x))
/* Proximity electrode baseline value */
#define REG_EPROXBV         0x2a
/* Max Half Delta Rising */
#define REG_MHDR            0x2b
/* Noise Half Delta Rising */
#define REG_NHDR            0x2c
/* Noise Count Limit Rising */
#define REG_NCLR            0x2d
/* Filter Delay Limit Rising */
#define REG_FDLR            0x2e
/* Max Half Delta Falling */
#define REG_MHDF            0x2f
/* Noise Half Delta Falling */
#define REG_NHDF            0x30
/* Noise Count Limit Falling */
#define REG_NCLF            0x31
/* Filter Delay Limit Falling */
#define REG_FDLF            0x32
/* Noise Half Delta Touched */
#define REG_NHDT            0x33
/* Noise Count Limit Touched */
#define REG_NCLT            0x34
/* Filter Delay Limit Touched */
#define REG_FDLT            0x35
/* Proximity Max Half Delta Rising */
#define REG_MHDPROXR        0x36
/* Proximity Noise Half Delta Rising */
#define REG_NHDPROXR        0x37
/* Proximity Noise Count Limit Rising */
#define REG_NCLPROXR        0x38
/* Proximity Filter Delay Limit Rising */
#define REG_FDLPROXR        0x39
/* Proximity Max Half Delta Falling */
#define REG_MHDPROXF        0x3a
/* Proximity Noise Half Delta Falling */
#define REG_NHDPROXF        0x3b
/* Proximity Noise Count Limit Falling */
#define REG_NCLPROXF        0x3c
/* Proximity Filter Delay Limit Falling */
#define REG_FDLPROXF        0x3d
/* Proximity Noise Half Delta Touched */
#define REG_NHDPROXT        0x3e
/* Proximity Noise Count Limit Touched */
#define REG_NCLPROXT        0x3f
/* Proximity Filter Delay Limit Touched */
#define REG_FDLPROXT        0x40
/* Eletrode Touch Threshold */
#define REG_ExTTH(x)        (0x41 + 2 * (x))
/* Eletrode Release Threshold */
#define REG_ExRTH(x)        (0x42 + 2 * (x))
/* Proximity Eletrode Touch Threshold */
#define REG_EPROXTTH        0x59
/* Proximity Eletrode Release Threshold */
#define REG_EPROXRTH        0x5a
/* Debounce Control */
#define REG_DEBOUNCE        0x5b
#define REG_DEBOUNCE__DR(dr)    ((dr) << 4)
#define REG_DEBOUNCE__DT(dt)    (dt)
/* Analog Front End Configuration */
#define REG_AFE             0x5c
#define REG_AFE__CDC(cdc)   (cdc)
#define REG_AFE__FFI(ffi)   ((ffi) << 6)
/* Filter Configuration */
#define REG_FILTER          0x5d
#define REG_FILTER__ESI(esi)    (esi)
#define REG_FILTER__SFI(sfi)    ((sfi) << 3)
#define REG_FILTER__CDT(cdt)    ((cdt) << 5)
/* Electrode Configuration */
#define REG_ELECTRODE       0x5e
#define REG_ELECTRODE__ELE_EN(en)       (en)
#define REG_ELECTRODE__ELEPROX_EN(en)   ((en) << 4)
#define REG_ELECTRODE__CL(cl)   ((cl) << 6)
/* Electrode X Current */
#define REG_CDCx(x)         (0x5f + (x))
/* Proximity Eletrode X Current */
#define REG_CDCPROX         0x6b
/* Electrode X Charge Time */
#define REG_CDTx(x)         (0x6c + (x) / 2)
#define REG_CDTx__CDT0(x)   (x)
#define REG_CDTx__CDT1(x)   ((x) << 4)
/* Proximity Eletrode X Charge Time  */
#define REG_CDTPROX         0x72
/* GPIO Control Register: CTL0{4-11} */
#define REG_GPIO_CTL0       0x73
#define REG_GPIO_CTL0__CTL0x(x) (1 << ((x) - 4))
/* GPIO Control Register: CTL1{4-11} */
#define REG_GPIO_CTL1       0x74
#define REG_GPIO_CTL1__CTL1x(x) (1 << ((x) - 4))
/* GPIO Data Register */
#define REG_GPIO_DATA       0x75
#define REG_GPIO_DATA__DATx(x)  (1 << ((x) - 4))
/* GPIO Direction Register */
#define REG_GPIO_DIR        0x76
#define REG_GPIO_DIR__DIRx(x)   (1 << ((x) - 4))
/* GPIO Enable Register */
#define REG_GPIO_EN         0x77
#define REG_GPIO_EN__ENx(x)     (1 << ((x) - 4))
/* GPIO Data Set Register */
#define REG_GPIO_SET        0x78
#define REG_GPIO_SET__SETx(x)   (1 << ((x) - 4))
/* GPIO Data Clear Register */
#define REG_GPIO_CLR        0x79
#define REG_GPIO_CLR__CLRx(x)   (1 << ((x) - 4))
/* GPIO Data Toggle Register */
#define REG_GPIO_TOG        0x7a
#define REG_GPIO_TOG__TOGx(x)   (1 << ((x) - 4))
/* Auto-Configuration Control 0 */
#define REG_AUTO_CONF       0x7b
#define REG_AUTO_CONF__ACE(ace)     (ace)
#define REG_AUTO_CONF__ARE(are)     ((are) << 1)
#define REG_AUTO_CONF__BVA(bva)     ((bva) << 2)
#define REG_AUTO_CONF__RETRY(retry) ((retry) << 4)
#define REG_AUTO_CONF__FFI(ffi)     ((ffi) << 6)
/* Auto-Configuration Control 1 */
#define REG_AUTO_CONF2      0x7c
#define REG_AUTO_CONF2__ACFIE(acfie)    (acfie)
#define REG_AUTO_CONF2__ARFIE(arfie)    ((arfie) << 1)
#define REG_AUTO_CONF2__OORIE(oorie)    ((oorie) << 2)
#define REG_AUTO_CONF2__SCTS(scts)      ((scts) << 7)
/* Auto-Configuration Upper-Limit */
#define REG_USL             0x7d
/* Auto-Configuration Lower-Limit */
#define REG_LSL             0x7e
/* Auto-Configuration Target Level */
#define REG_TL              0x7f
/* Soft-Reset */
#define REG_SOFTRESET       0x80
#define REG_SOFTRESET__MAGIC    0x63
/* PWM Control */
#define REG_PWMx(x)         (0x81 + ((x) - 4) / 2)
#define REG_PWMx_IS_PWM0(x) (((x) % 2) == 0)
#define REG_PWMx__PWM0(x)   (x)
#define REG_PWMx__PWM0_BM   0xf
#define REG_PWMx__PWM1(x)   ((x) << 4)
#define REG_PWMx__PWM1_BM   0xf0

#endif /* __MPR121_H__ */
