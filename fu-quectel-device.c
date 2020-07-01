/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-usb-device.h"
#include "fu-device.h"

#include "fu-quectel-device.h"
#include "errno.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"

guint32
fu_quectel_device_write_firmware(void) {
    return 0;
}

gboolean
fu_quectel_device_get_version(gchar *version) {
    g_autofree gchar *ver = NULL;
    int len = 128;
    int ret;
    int ttyhandler;

    ver = g_malloc0(len);
    g_return_val_if_fail(ver != NULL, FALSE);
    ttyhandler = open(tty_dev, O_NOCTTY|O_NONBLOCK|O_RDWR);
    if (ttyhandler == -1) {
        g_printerr("Fail to open device %s\r\n", tty_dev);
        return FALSE;
    }
    g_return_val_if_fail(fu_quectel_command_send(ttyhandler, CMD_AT, AT_VERSION), FALSE);
    
    /* 
    ATI
    Quectel
    EC20F
    Revision: EC20CEFDR02A15M4G

    OK
    */
    do {
        bzero(ver, len);
        ret = fu_quectel_command_resp(ttyhandler, ver, len);
        g_print("resp: %s", ver);
        if (ret < 0) {close(ttyhandler); return FALSE;}
        if (ret == 0) break;
        if (ret == 1) continue;
        if (g_str_has_prefix (ver, "Revision: ")) break;
        if (*(ver + ret) = 0 && g_strv_contains (ver, "Revision: ")) break;
    } while(1);
    sscanf(g_strrstr(ver, "Revision: ") + strlen("Revision: "), "%s", version);
    close(ttyhandler);
    return TRUE;
}

gboolean
fu_quectel_device_enter_fastboot(void) {
    g_return_val_if_fail(fu_quectel_command_send(-1, CMD_AT, AT_FASTBOOT), FALSE);
    return TRUE;
}

gboolean
fu_quectel_device_quit_fastboot(void) {
    g_return_val_if_fail(fu_quectel_command_send(-1, CMD_FB, FB_QUIT), FALSE);
    return TRUE;
}

void
fu_quectel_device_set_info (FuDevice *device, guint32 vid, guint32 pid) {
	fu_device_set_id (device, "Quectel IoT Module");
	fu_device_set_name (device, "Quectel IoT(TM)");
	fu_device_add_icon (device, "preferences-desktop-keyboard");
	fu_device_set_vendor (device, "Quectel Corp.");
	fu_device_set_vendor_id (device, "USB:0x2C7C");

	/* 'USB\VID_XXX&PID_XXX' */
    fu_quectel_calc_guid(vid, pid);
	// fu_device_add_guid(device, fu_quectel_calc_guid(vid, pid));
	fu_device_add_guid(device, "475d9bbd-1b7a-554e-8ca7-54985174a962");
	fu_quectel_device_set_version(device);

	fu_device_add_flag (device, FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_set_version_bootloader (device, "0.1.2");
	fu_device_set_version_lowest (device, "0.0.0");
}

gboolean
fu_quectel_device_version_code(const gchar *ver_str, gchar **version) {
    int major = 0;
    int minor = 0;
    
    // sscanf("EC20CEFDR02A15M4G", "%*[A-Z]%*[0-9]%*[A-Z]%dA%d", &a, &b);
    // RxxAxx
    g_print("Get version from %s\r\n", ver_str);
    sscanf("EC20CEFDR02A15M4G", "%*[A-Z]%*[0-9]%*[A-Z]%dA%d", &major, &minor);
    if (major != 0 && minor != 0) {
        *version = g_strdup_printf("1.%d.%d", major, minor);
        return TRUE;
    }
    return FALSE;
}

void
fu_quectel_device_set_version (FuDevice *device) {
	g_autofree gchar *summary = NULL;
    g_autofree gchar *version = NULL;

	summary = g_malloc0(64+1);
	if (summary == NULL) {
		g_printerr("Failed to malloc...");
		return;
	}

	g_return_if_fail(fu_quectel_device_get_version(summary));
	fu_device_set_summary (device, summary);
    fu_quectel_device_version_code(summary, &version);
	fu_device_set_version (device, version);
}