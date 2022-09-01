/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id:  $
 *
 * Copyright (C) 2008 by Frank Gevaerts
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
#include "string.h"
#include "system.h"
#include "usb_core.h"
#include "usb_drv.h"
#include "kernel.h"
#include "usb_charging_only.h"
#include "usb_class_driver.h"

/*#define LOGF_ENABLE*/
#include "logf.h"

/* charging_only interface */
static struct usb_interface_descriptor __attribute__((aligned(2)))
                                interface_descriptor =
{
    .bLength            = sizeof(struct usb_interface_descriptor),
    .bDescriptorType    = USB_DT_INTERFACE,
    .bInterfaceNumber   = 0,
    .bAlternateSetting  = 0,
    .bNumEndpoints      = 0,
    .bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface         = 0
};

static int usb_interface;

int usb_charging_only_request_endpoints(struct usb_class_driver *drv)
{
    (void) drv;
    return 0;
}

int usb_charging_only_set_first_interface(int interface)
{
    usb_interface = interface;
    return interface + 1;
}

int usb_charging_only_get_config_descriptor(unsigned char *dest,int max_packet_size)
{
    (void)max_packet_size;
    unsigned char *orig_dest = dest;

    interface_descriptor.bInterfaceNumber=usb_interface;
    PACK_DATA(&dest, interface_descriptor);

    return (dest-orig_dest);
}
