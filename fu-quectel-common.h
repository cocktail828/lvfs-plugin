/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __FU_QUECTEL_COMMON_H
#define __FU_QUECTEL_COMMON_H

#include "config.h"

#include "fu-plugin.h"
#include "fu-plugin-vfuncs.h"

#define dbg(fmt, arg...) do{ \
		g_print("[%d] "fmt"\r\n", __LINE__, ##arg); \
	}while(0)

#define QUECTEL_VID 0x2c7c
typedef struct QuectelDevice {
	guint32 vid;
	guint32 pid;
	gchar *product;
}QDev;

typedef enum CMD_TYPE {
    CMD_AT,
    CMD_FB,
}CMDType;

void fu_quectel_conv_version_code(guint32 ver_code, gchar **ver_str) ;
gchar* fu_quectel_calc_guid(guint32 vid, guint32 pid) ;
gboolean fu_quectel_is_my_dev(guint32 vid, guint32 pid) ;
/* dump at command */
gboolean fu_quectel_command_send(int ttyhandler, CMDType type, gchar *cmd);
gboolean fu_quectel_command_resp(int ttyhandler, gchar *resp, int size);

void fu_quectel_release_ttyhandler(int handler);
void fu_quectel_set_ttyhandler(const gchar *path, const gchar *name);
gboolean fu_quectel_get_product(guint32 vid, guint32 pid, gchar **product);
gboolean fu_quectel_check_vid_pid(const gchar *path, const guint32 vid, const guint32 pid);

extern gchar *tty_dev;
#endif /* __FU_QUECTEL_COMMON_H*/