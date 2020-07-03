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

#define FuQuectelUSBHandle(fu) (((struct FuPluginData *)fu_plugin_get_data(fu))->usbdev)
#define FuQuectelFirmware(fu) (((struct FuPluginData *)fu_plugin_get_data(fu))->firmware)

struct FuPluginData
{
	GMutex mutex;
	gint total_operation_num;
	gint current_operation_num;
	QuectelUSBDev *usbdev;
	QuectelFirmware *firmware;
};

void fu_plugin_init(FuPlugin *plugin)
{
	fu_plugin_set_build_hash(plugin, FU_BUILD_HASH);
	fu_plugin_alloc_data(plugin, sizeof(FuPluginData));
	fu_plugin_set_device_gtype(plugin, fu_usb_device_get_type());
}

void fu_plugin_destroy(FuPlugin *plugin)
{
	FuPluginData *data = fu_plugin_get_data(plugin);
	if (data && data->usbdev)
		free(data->usbdev);

	if (data)
		g_free(data);
}

gboolean
fu_quectel_set_info(FuPlugin *plugin, FuDevice *device)
{
	FuPluginData *data = fu_plugin_get_data(plugin);
	QuectelUSBDev *usbdev = fu_quectel_find_usb_device();
	data->usbdev = usbdev;

	if (!usbdev)
		return FALSE;

	g_autofree gchar *vendor_id = g_strdup_printf("USB:%04x:%04x", usbdev->idVendor, usbdev->idProduct);
	g_autofree gchar *device_id = g_strdup_printf("%s-%s", fu_plugin_get_name(plugin), usbdev->devpath);

	fu_device_set_id(device, device_id);
	fu_device_add_parent_guid(device, QUECTEL_COMMON_USB_GUID);
	fu_device_add_guid(device, QUECTEL_USB_EC20CEFA_GUID);
	fu_device_set_name(device, "Quectel Mobile Broadband Devices");
	fu_device_add_icon(device, "network-modem");
	fu_device_set_protocol(device, "com.qualcomm.firehose");
	fu_device_set_summary(device, "Quectel 5G/4G Series Modems");
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_CAN_VERIFY);
	fu_device_set_vendor(device, "Quectel.");
	fu_device_set_vendor_id(device, vendor_id);
	fu_device_set_version_bootloader(device, "0.1.2");
	fu_device_set_version(device, "1.2.2", FWUPD_VERSION_FORMAT_PLAIN);
	fu_device_set_version_lowest(device, "1.2.0");

	return TRUE;
}

gboolean
fu_plugin_coldplug(FuPlugin *plugin, GError **error)
{
	gboolean status;
	g_autoptr(FuDevice) device = NULL;
	device = fu_device_new();
	status = fu_quectel_set_info(plugin, device);
	fu_plugin_device_add(plugin, device);

	return status;
}

gboolean
fu_plugin_update_attach(FuPlugin *plugin, FuDevice *device, GError **error)
{
	return fu_quectel_set_info(plugin, device);
}

/**
 * send EDL command, modem will switch into EDL mode(bootloader)
 */
gboolean
fu_plugin_update_detach(FuPlugin *plugin, FuDevice *device, GError **error)
{
	QuectelUSBDev *usbdev = FuQuectelUSBHandle(plugin);
	gboolean status = fu_quectel_usb_device_switch_mode(usbdev);
	LOGI("%s switch info EDL mode", status ? "successfully" : "fail");
	if (!status)
		return status;

	fu_device_add_flag(device, FWUPD_STATUS_DOWNLOADING);
	return TRUE;
}

void fu_plugin_device_registered(FuPlugin *plugin, FuDevice *device)
{
	// this will be called if coldplug return TRUE
	fu_device_set_metadata(device, "BestDevice", "/dev/urandom");
}

gboolean
fu_plugin_verify(FuPlugin *plugin,
				 FuDevice *device,
				 FuPluginVerifyFlags flags,
				 GError **error)
{
	LOGI("%s", __func__);
	LOGI("%s", fu_device_get_version(device));
	return TRUE;
}

gboolean
fu_plugin_update(FuPlugin *plugin,
				 FuDevice *device,
				 GBytes *blob_fw,
				 FwupdInstallFlags flags,
				 GError **error)
{
	gboolean status;
	QuectelUSBDev *usbdev = FuQuectelUSBHandle(plugin);
	QuectelFirmware *fm = FuQuectelFirmware(plugin);

	// first stage: transfer prog*.mbn via sahara protocol
	status = fu_quectel_usb_device_sahara_write(usbdev, fm);
	LOGI("%s transfer firehose flash tool", status ? "successfully" : "fail");
	if (!status)
		return status;

	// second stage: transfer prog*.mbn via sahara protocol
	status = fu_quectel_usb_device_firehose_write(usbdev, fm);
	LOGI("%s transfer main programs", status ? "successfully" : "fail");
	if (!status)
		return status;

	// third stage: reset device
	fu_quectel_usb_device_firehose_reset(usbdev);
	fu_device_add_flag(device, FWUPD_UPDATE_STATE_SUCCESS);
	return TRUE;
}

gboolean
fu_plugin_get_results(FuPlugin *plugin, FuDevice *device, GError **error)
{
	fu_device_set_update_state(device, FWUPD_UPDATE_STATE_SUCCESS);
	fu_device_set_update_error(device, NULL);
	return TRUE;
}
