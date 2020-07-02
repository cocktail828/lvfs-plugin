/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-hash.h"

#include <syslog.h>

#include "fu-quectel-common.h"

struct FuPluginData
{
	GMutex mutex;
	gint total_operation_num;
	gint current_operation_num;
};

void fu_plugin_init(FuPlugin *plugin)
{
	fu_plugin_set_build_hash(plugin, FU_BUILD_HASH);
	fu_plugin_alloc_data(plugin, sizeof(FuPluginData));
	fu_plugin_set_device_gtype(plugin, fu_usb_device_get_type());
}

void fu_plugin_destroy(FuPlugin *plugin)
{
	FuPluginData *data = fu_plugin_get_data (plugin);
	if (data)
		g_free(data);
}

gboolean
fu_plugin_usb_device_added (FuPlugin *plugin,
			    FuUsbDevice *device,
			    GError **error)
{
	// FuPluginData *data = fu_plugin_get_data (plugin);
	// guint16 pid;
	// guint16 vid;
	// const gchar *platform = NULL;

	// vid = fu_usb_device_get_vid (device);
	// pid = fu_usb_device_get_pid (device);
	// platform = fu_device_get_physical_id (FU_DEVICE (device));
	LOGI("%s", __func__);

	return TRUE;
}

gboolean
fu_plugin_coldplug(FuPlugin *plugin, GError **error)
{
	static gboolean ex = FALSE;
	if (ex)
		return FALSE;

	ex = TRUE;
	LOGI("%s", __func__);
	g_autoptr(FuDevice) device = NULL;
	device = fu_device_new();

	g_autofree gchar *uniqid = g_strdup_printf("%s-%d", fu_plugin_get_name(plugin), 1);
	fu_device_set_id(device, uniqid);
	fu_device_add_parent_guid(device, QUECTEL_COMMON_USB_GUID);
	fu_device_add_guid(device, QUECTEL_COMMON_USB_GUID);
	fu_device_set_name(device, "Quectel Mobile Broadband Devices");
	fu_device_add_icon(device, "network-modem");
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_CAN_VERIFY);
	fu_device_set_protocol(device, "com.qualcomm.firehose");
	fu_device_set_summary(device, "Quectel 5G/4G Series Modems");
	fu_device_set_vendor(device, "Quectel.");
	fu_device_set_vendor_id(device, "USB:0x2C7C");
	fu_device_set_version_bootloader(device, "0.1.2");
	fu_device_set_version(device, "1.2.2", FWUPD_VERSION_FORMAT_PLAIN);
	fu_device_set_version_lowest(device, "1.2.0");
	fu_plugin_device_add(plugin, device);

	return TRUE;
}

gboolean
fu_plugin_update_attach (FuPlugin *plugin, FuDevice *device, GError **error) {
	LOGI("%s", __func__);
	return TRUE;
}

gboolean
fu_plugin_update_detach (FuPlugin *plugin, FuDevice *device, GError **error) {
	LOGI("%s", __func__);

	for (guint i = 1; i <= 100; i++) {
		g_usleep (1000);
		fu_device_set_progress(device, i);
	}
	LOGI("successfully switch info EDL mode, detach device");
	return TRUE;
}

void fu_plugin_device_registered(FuPlugin *plugin, FuDevice *device)
{
	static gboolean ex = FALSE;
	if (ex)
		return FALSE;

	ex = TRUE;
	fu_device_set_summary(device, "registered");
	fu_device_set_metadata(device, "BestDevice", "/dev/urandom");
	LOGI("%s", __func__);
}

static gchar *
fu_plugin_test_get_version (GBytes *blob_fw)
{
	LOGI("%s", __func__);
	return "";
}

gboolean
fu_plugin_verify(FuPlugin *plugin,
				 FuDevice *device,
				 FuPluginVerifyFlags flags,
				 GError **error)
{
	LOGI("%s", __func__);
	LOGI("%s", fu_device_get_version (device));
	return TRUE;
}

gboolean
fu_plugin_update(FuPlugin *plugin,
				 FuDevice *device,
				 GBytes *blob_fw,
				 FwupdInstallFlags flags,
				 GError **error)
{
	LOGI("%s", __func__);
	return TRUE;
}

gboolean
fu_plugin_get_results(FuPlugin *plugin, FuDevice *device, GError **error)
{
	fu_device_set_update_state(device, FWUPD_UPDATE_STATE_SUCCESS);
	fu_device_set_update_error(device, NULL);
	return TRUE;
}
