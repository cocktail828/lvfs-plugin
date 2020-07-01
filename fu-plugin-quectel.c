/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-hash.h"

#include "fu-quectel-common.h"
#include "fu-quectel-device.h"

#define MAX_DEVICE_NUM 8
struct FuPluginData {
	GMutex			 mutex;
	int device_num;
	int ttyhandler[MAX_DEVICE_NUM];
	gchar *guid_str[MAX_DEVICE_NUM];
};

void
fu_plugin_init (FuPlugin *plugin)
{
	dbg("");
	fu_plugin_set_build_hash (plugin, FU_BUILD_HASH);
	fu_plugin_alloc_data (plugin, sizeof (FuPluginData));
	g_debug ("quectel init");
}

void
fu_plugin_destroy (FuPlugin *plugin)
{
	//FuPluginData *data = fu_plugin_get_data (plugin);
	g_debug ("quectel destroy");
}

gboolean
fu_plugin_usb_device_added (FuPlugin *plugin, FuUsbDevice *usb_device, GError **error) {
	guint16 vid;
	guint32 pid;
	g_autoptr(FuDevice) device = NULL;

	g_debug ("quectel usb device added");
	vid = fu_usb_device_get_vid(usb_device);
	pid = fu_usb_device_get_pid(usb_device);
	if (fu_quectel_is_my_dev(vid, pid) == FALSE) {
		return TRUE;
	}

	device = fu_device_new ();
	fu_quectel_device_set_info(device, vid, pid);
	fu_plugin_device_register (plugin, device);
	if (fu_device_get_metadata (device, "quec_dev") == NULL) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_NOT_FOUND,
			     "Device not set by another plugin");
		return FALSE;
	}

	fu_plugin_device_add (plugin, device);
	return TRUE;
}

void
fu_plugin_device_registered (FuPlugin *plugin, FuDevice *device) {
	fu_device_set_metadata (device, "quec_dev", "working");
	g_debug ("quectel usb device register");
}

gboolean
fu_plugin_verify (FuPlugin *plugin,
		  FuDevice *device,
		  FuPluginVerifyFlags flags,
		  GError **error)
{
	if (g_strcmp0 (fu_device_get_version (device), "1.2.0") == 0) {
		fu_device_add_checksum (device, "7998cd212721e068b2411135e1f90d0ad436d730");
		return TRUE;
	}
	g_set_error (error,
		     FWUPD_ERROR,
		     FWUPD_ERROR_NOT_SUPPORTED,
		     "no checksum for %s", fu_device_get_version (device));
	return FALSE;
}

gboolean
fu_plugin_update (FuPlugin *plugin,
		  FuDevice *device,
		  GBytes *blob_fw,
		  FwupdInstallFlags flags,
		  GError **error)
{
	dbg("update");
	if (g_strcmp0 (g_getenv ("FWUPD_PLUGIN_TEST"), "fail") == 0) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "device was not in supported mode");
		return FALSE;
	}
	fu_device_set_status (device, FWUPD_STATUS_DECOMPRESSING);
	for (guint i = 1; i <= 100; i++) {
		g_usleep (1000);
		fu_device_set_progress (device, i);
	}
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_WRITE);
	for (guint i = 1; i <= 100; i++) {
		g_usleep (1000);
		fu_device_set_progress (device, i);
	}
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_VERIFY);
	for (guint i = 1; i <= 100; i++) {
		g_usleep (1000);
		fu_device_set_progress (device, i);
	}

	/* upgrade, or downgrade */
	if (flags & FWUPD_INSTALL_FLAG_ALLOW_OLDER) {
		fu_device_set_version (device, "1.2.2");
	} else {
		fu_device_set_version (device, "1.2.3");
	}
	return TRUE;
}

gboolean
fu_plugin_get_results (FuPlugin *plugin, FuDevice *device, GError **error)
{
	fu_device_set_update_state (device, FWUPD_UPDATE_STATE_SUCCESS);
	fu_device_set_update_error (device, NULL);
	return TRUE;
}
