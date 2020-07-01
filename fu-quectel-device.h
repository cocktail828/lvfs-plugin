/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */
#ifndef __FU_QUECTEL_DEVICE_H
#define __FU_QUECTEL_DEVICE_H

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-quectel-common.h"

#define AT_VERSION "ATI\r\n"
#define AT_FASTBOOT "AT+QFASTBOOT\r\n"
#define FB_QUIT "fastboot reboot\r\n"

guint32 fu_quectel_device_write_firmware(void);
gboolean fu_quectel_device_get_version(gchar *version);
gboolean fu_quectel_device_enter_fastboot(void);
gboolean fu_quectel_device_quit_fastboot(void);
void fu_quectel_device_set_info (FuDevice *device, guint32 vid, guint32 pid);
void fu_quectel_device_set_version (FuDevice *device);
gboolean fu_quectel_device_version_code(const gchar *ver_str, gchar **version);
#endif /* __FU_QUECTEL_DEVICE_H */