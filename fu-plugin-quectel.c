/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-hash.h"

struct FuPluginData
{
	GMutex mutex;
};

#define QUECTEL_PARENT_GUID "22ae45db-f68e-5c55-9c02-4557dca238ec"

void fu_plugin_init(FuPlugin *plugin)
{
	fu_plugin_set_build_hash(plugin, FU_BUILD_HASH);
	fu_plugin_alloc_data(plugin, sizeof(FuPluginData));
	fu_plugin_set_device_gtype(plugin, fu_usb_device_get_type());
	g_debug(__func__);
}

void fu_plugin_destroy(FuPlugin *plugin)
{
	//FuPluginData *data = fu_plugin_get_data (plugin);
	g_debug("destroy");
}


gboolean
fu_plugin_usb_device_added (FuPlugin *plugin,
			    FuUsbDevice *device,
			    GError **error)
{
	FuPluginData *data = fu_plugin_get_data (plugin);
	guint16 pid;
	guint16 vid;
	const gchar *platform = NULL;

	vid = fu_usb_device_get_vid (device);
	pid = fu_usb_device_get_pid (device);
	platform = fu_device_get_physical_id (FU_DEVICE (device));

	return TRUE;
}

gboolean
fu_plugin_coldplug(FuPlugin *plugin, GError **error)
{
	g_autoptr(FuDevice) device = NULL;
	device = fu_device_new();

	fu_device_set_id(device, "Quectel");
	fu_device_add_parent_guid(device, QUECTEL_PARENT_GUID);
	fu_device_add_guid(device, QUECTEL_PARENT_GUID);
	fu_device_set_name(device, "Quectel Mobile Broadband Devices");
	fu_device_add_icon(device, "network-modem");
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag(device, FWUPD_DEVICE_FLAG_CAN_VERIFY);
	fu_device_set_protocol(device, "com.qualcomm.firehose");
	fu_device_set_summary(device, "Quectel 5G/4G Series Modems");
	fu_device_set_vendor(device, "Quectel.");
	fu_device_set_vendor_id(device, "USB:0x2C7C");
	fu_device_set_version_bootloader(device, "0.1.2");
	fu_device_set_version(device, "1.2.2", FWUPD_VERSION_FORMAT_TRIPLET);
	fu_device_set_version_lowest(device, "1.2.0");
	fu_plugin_device_add(plugin, device);

	fu_plugin_device_register(plugin, device);

	return TRUE;
}

void fu_plugin_device_registered(FuPlugin *plugin, FuDevice *device)
{
	fu_device_set_summary(device, "registered");
	fu_device_set_metadata(device, "BestDevice", "/dev/urandom");
}

gboolean
fu_plugin_verify(FuPlugin *plugin,
				 FuDevice *device,
				 FuPluginVerifyFlags flags,
				 GError **error)
{
	if (g_strcmp0(fu_device_get_version(device), "1.2.2") == 0)
	{
		fu_device_add_checksum(device, "90d0ad436d21e0687998cd2127b2411135e1f730");
		fu_device_add_checksum(device, "921631916a60b295605dbae6a0309f9b64e2401b3de8e8506e109fc82c586e3a");
		return TRUE;
	}
	if (g_strcmp0(fu_device_get_version(device), "1.2.3") == 0)
	{
		fu_device_add_checksum(device, "7998cd212721e068b2411135e1f90d0ad436d730");
		fu_device_add_checksum(device, "dbae6a0309b3de8e850921631916a60b2956056e109fc82c586e3f9b64e2401a");
		return TRUE;
	}
	if (g_strcmp0(fu_device_get_version(device), "1.2.4") == 0)
	{
		fu_device_add_checksum(device, "2b8546ba805ad10bf8a2e5ad539d53f303812ba5");
		fu_device_add_checksum(device, "b546c241029ce4e16c99eb6bfd77b86e4490aa3826ba71b8a4114e96a2d69bcd");
		return TRUE;
	}
	g_set_error(error,
				FWUPD_ERROR,
				FWUPD_ERROR_NOT_SUPPORTED,
				"no checksum for %s", fu_device_get_version(device));
	return FALSE;
}

static gchar *
fu_plugin_test_get_version(GBytes *blob_fw)
{
	const gchar *str = g_bytes_get_data(blob_fw, NULL);
	guint64 val = 0;
	if (str == NULL)
		return NULL;
	val = fu_common_strtoull(str);
	if (val == 0x0)
		return NULL;
	return fu_common_version_from_uint32(val, FWUPD_VERSION_FORMAT_TRIPLET);
}

gboolean
fu_plugin_update(FuPlugin *plugin,
				 FuDevice *device,
				 GBytes *blob_fw,
				 FwupdInstallFlags flags,
				 GError **error)
{
	const gchar *test = g_getenv("FWUPD_PLUGIN_TEST");
	gboolean requires_activation = g_strcmp0(test, "requires-activation") == 0;
	if (g_strcmp0(test, "fail") == 0)
	{
		g_set_error_literal(error,
							FWUPD_ERROR,
							FWUPD_ERROR_NOT_SUPPORTED,
							"device was not in supported mode");
		return FALSE;
	}
	fu_device_set_status(device, FWUPD_STATUS_DECOMPRESSING);
	for (guint i = 1; i <= 100; i++)
	{
		g_usleep(1000);
		fu_device_set_progress(device, i);
	}
	fu_device_set_status(device, FWUPD_STATUS_DEVICE_WRITE);
	for (guint i = 1; i <= 100; i++)
	{
		g_usleep(1000);
		fu_device_set_progress(device, i);
	}
	fu_device_set_status(device, FWUPD_STATUS_DEVICE_VERIFY);
	for (guint i = 1; i <= 100; i++)
	{
		g_usleep(1000);
		fu_device_set_progress(device, i);
	}

	/* composite test, upgrade composite devices */
	if (g_strcmp0(test, "composite") == 0)
	{
		if (g_strcmp0(fu_device_get_logical_id(device), "child1") == 0)
		{
			fu_device_set_version(device, "2", FWUPD_VERSION_FORMAT_PLAIN);
			return TRUE;
		}
		else if (g_strcmp0(fu_device_get_logical_id(device), "child2") == 0)
		{
			fu_device_set_version(device, "11", FWUPD_VERSION_FORMAT_PLAIN);
			return TRUE;
		}
	}

	/* upgrade, or downgrade */
	if (requires_activation)
	{
		fu_device_add_flag(device, FWUPD_DEVICE_FLAG_NEEDS_ACTIVATION);
	}
	else
	{
		g_autofree gchar *ver = fu_plugin_test_get_version(blob_fw);
		if (ver != NULL)
		{
			fu_device_set_version(device, ver, FWUPD_VERSION_FORMAT_TRIPLET);
		}
		else
		{
			if (flags & FWUPD_INSTALL_FLAG_ALLOW_OLDER)
			{
				fu_device_set_version(device, "1.2.2", FWUPD_VERSION_FORMAT_TRIPLET);
			}
			else
			{
				fu_device_set_version(device, "1.2.3", FWUPD_VERSION_FORMAT_TRIPLET);
			}
		}
	}

	/* do this all over again */
	if (g_strcmp0(test, "another-write-required") == 0)
	{
		g_unsetenv("FWUPD_PLUGIN_TEST");
		fu_device_add_flag(device, FWUPD_DEVICE_FLAG_ANOTHER_WRITE_REQUIRED);
	}

	/* for the self tests only */
	fu_device_set_metadata_integer(device, "nr-update",
								   fu_device_get_metadata_integer(device, "nr-update") + 1);

	return TRUE;
}

gboolean
fu_plugin_activate(FuPlugin *plugin, FuDevice *device, GError **error)
{
	fu_device_set_version(device, "1.2.3", FWUPD_VERSION_FORMAT_TRIPLET);
	return TRUE;
}

gboolean
fu_plugin_get_results(FuPlugin *plugin, FuDevice *device, GError **error)
{
	fu_device_set_update_state(device, FWUPD_UPDATE_STATE_SUCCESS);
	fu_device_set_update_error(device, NULL);
	return TRUE;
}

gboolean
fu_plugin_composite_prepare(FuPlugin *plugin,
							GPtrArray *devices,
							GError **error)
{
	if (g_strcmp0(g_getenv("FWUPD_PLUGIN_TEST"), "composite") == 0)
	{
		for (guint i = 0; i < devices->len; i++)
		{
			FuDevice *device = g_ptr_array_index(devices, i);
			fu_device_set_metadata(device, "frimbulator", "1");
		}
	}
	return TRUE;
}

gboolean
fu_plugin_composite_cleanup(FuPlugin *plugin,
							GPtrArray *devices,
							GError **error)
{
	if (g_strcmp0(g_getenv("FWUPD_PLUGIN_TEST"), "composite") == 0)
	{
		for (guint i = 0; i < devices->len; i++)
		{
			FuDevice *device = g_ptr_array_index(devices, i);
			fu_device_set_metadata(device, "frombulator", "1");
		}
	}
	return TRUE;
}
