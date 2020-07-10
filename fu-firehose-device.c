/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <string.h>
#include <xmlb.h>

#include "fu-archive.h"
#include "fu-chunk.h"
#include "fu-firehose-device.h"
#include "fu-firehose-protocol.h"
#include "fu-sahara-protocol.h"

#define FIREHOSE_REMOVE_DELAY_RE_ENUMERATE	60000 /* ms */
#define FIREHOSE_TRANSACTION_TIMEOUT		1000 /* ms */
#define FIREHOSE_TRANSACTION_RETRY_MAX		600
#define FIREHOSE_EP_IN				0x81
#define FIREHOSE_EP_OUT				0x01

#define FIREHOSE_EDL_VID            0x05c6
#define FIREHOSE_EDL_PID            0x9008

#define MAX_RX_SIZE                (4 * 1024)
#define MAX_TX_SIZE                (8 * 1024)

#define SAHARA_VERSION 0x02
#define SAHARA_VERSION_COMPATIBLE 0

#define FIREHOSE_TOOL_PREFIX     "prog_"
#define FIREHOSE_XML_PREFIX      "rawprogram_"

struct _FuFirehoseDevice {
	FuUsbDevice			 parent_instance;
	guint				 max_tx_size;
	guint				 max_rx_size;
	guint                intf_nr;
};

G_DEFINE_TYPE (FuFirehoseDevice, fu_firehose_device, FU_TYPE_USB_DEVICE)

static void
fu_firehose_device_to_string (FuDevice *device, guint idt, GString *str)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	fu_common_string_append_kx (str, idt, "MaxPayloadSizeToTargetInBytes", self->max_tx_size);
	fu_common_string_append_kx (str, idt, "MaxPayloadSizeFromTargetInBytes", self->max_rx_size);
}

static gboolean
fu_firehose_device_probe (FuDevice *device, GError **error)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (self));
	g_autoptr(GUsbInterface) intf = NULL;

	/* find the correct firehose interface */
	if (FIREHOSE_EDL_VID == g_usb_device_get_vid(usb_device) &&
		FIREHOSE_EDL_PID == g_usb_device_get_pid(usb_device)) {
	    self->intf_nr = 0; // only one interface appear in EDL mode
	    return TRUE;
	}
	return FALSE;
}

static gboolean
fu_firehose_device_open (FuUsbDevice *device, GError **error)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	GUsbDevice *usb_device = fu_usb_device_get_dev (device);

	if (!g_usb_device_claim_interface (usb_device, self->intf_nr,
					   G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					   error)) {
		g_prefix_error (error, "failed to claim interface: ");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static void
fu_firehose_buffer_dump (const gchar *title, const guint8 *buf, gsize sz)
{
	if (g_getenv ("FWUPD_FIREHOSE_VERBOSE") == NULL)
		return;
	g_print ("%s (%" G_GSIZE_FORMAT "):\n", title, sz);
	for (gsize i = 0; i < sz; i++) {
		g_print ("%02x[%c] ", buf[i], g_ascii_isprint (buf[i]) ? buf[i] : '?');
		if (i > 0 && (i + 1) % 256 == 0)
			g_print ("\n");
	}
	g_print ("\n");
}

static gboolean
fu_firehose_device_write (FuDevice *device, const guint8 *buf, gsize buflen, GError **error)
{
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (device));
	g_autofree guint8 *buf2 = g_memdup (buf, (guint) buflen);
	gsize actual_len = 0;
	gboolean ret;

	fu_firehose_buffer_dump ("writing", buf, buflen);
	ret = g_usb_device_bulk_transfer (usb_device,
					  FIREHOSE_EP_OUT,
					  buf2,
					  buflen,
					  &actual_len,
					  FIREHOSE_TRANSACTION_TIMEOUT,
					  NULL, error);
	if (!ret) {
		g_prefix_error (error, "failed to do bulk out transfer: ");
		return FALSE;
	}
	if (actual_len != buflen) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "only wrote %" G_GSIZE_FORMAT "bytes", actual_len);
		return FALSE;
	}
	return TRUE;
}

typedef enum {
	FU_FIREHOSE_DEVICE_READ_FLAG_NONE,
	FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
} FuFirehoseDeviceReadFlags;

static gboolean
fu_firehose_device_read (FuDevice *device,
			 gchar **value_out,
			 FuFirehoseDeviceReadFlags flags,
			 GError **error)
{
	// FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (device));
	g_autoptr(GPtrArray) parts = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	guint retries = 1;

	/* these commands may return INFO or take some time to complete */
	if (flags & FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL)
		retries = FIREHOSE_TRANSACTION_RETRY_MAX;

	for (guint i = 0; i < retries; i++) {
		gboolean ret;
		gsize actual_len = 0;
		guint8 buf[MAX_RX_SIZE] = { 0x00 };
		g_autoptr(GError) error_local = NULL;

		ret = g_usb_device_bulk_transfer (usb_device,
						  FIREHOSE_EP_IN,
						  buf,
						  sizeof(buf),
						  &actual_len,
						  FIREHOSE_TRANSACTION_TIMEOUT,
						  NULL, &error_local);
		if (!ret) {
			if (g_error_matches (error_local,
					     G_USB_DEVICE_ERROR,
					     G_USB_DEVICE_ERROR_TIMED_OUT)) {
				LOGI ("ignoring %s", error_local->message);
				continue;
			}
			g_propagate_prefixed_error (error,
						g_steal_pointer (&error_local),
					    "failed to do bulk in transfer: ");
			return FALSE;
		}
		fu_firehose_buffer_dump ("read", buf, actual_len);
		if (actual_len < 4) {
			g_set_error (error,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"only read %" G_GSIZE_FORMAT "bytes",
						actual_len);
			return FALSE;
		}

		/* info */
		if (!xb_builder_source_load_bytes (source,
					g_bytes_new(buf, actual_len),
					XB_BUILDER_SOURCE_FLAG_NONE, error))
			return FALSE;

		xb_builder_import_source (builder, source);
		silo = xb_builder_compile (builder,
					XB_BUILDER_COMPILE_FLAG_NONE,
					NULL,
					error);
		if (silo == NULL)
			return FALSE;

		LOGI ("%s", buf);
		/* <?xml version="1.0" ?>
		 *	<data>
		 *	<response value="ACK" />
		 *	</data>
		 */
		parts = xb_silo_query (silo, "data/response", 0, error);
		if (parts) {
			const gchar *value = xb_node_get_attr(
						g_ptr_array_index (parts, 0),
						"value");
			if (value_out)
				*value_out = g_strdup(value);
			if (value && g_strcmp0(value, "ACK") == 0)
				return TRUE;
			if (value && g_strcmp0(value, "NAK") == 0)
				return FALSE;
		}
		g_clear_error(error);

		/* <?xml version="1.0" encoding="UTF-8" ?>
			<data>
			<log value="Hash start sector 0 num sectors 131072" />
			</data> */
		parts = xb_silo_query (silo, "data/log", 0, error);
		if (parts) {
			const gchar *value = xb_node_get_attr(
						g_ptr_array_index (parts, 0),
						"value");
			/* <log ... /> */
			if (value_out)
				*value_out = g_strdup(value);
			return TRUE;
		}
		g_clear_error(error);

		/* unknown failure */
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "failed to read response");
		return FALSE;
	}

	/* we timed out a *lot* */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "no response to read");
	return FALSE;
}

static gboolean
fu_firehose_device_cmd (FuDevice *device, const gchar *cmd,
			FuFirehoseDeviceReadFlags flags, GError **error)
{
	gsize buflen = (cmd != NULL) ? strlen (cmd) : 0;

	LOGI ("%s", cmd);
	if (cmd && !fu_firehose_device_write (device, (const guint8 *) cmd, buflen, error))
		return FALSE;

	do {
		g_autofree gchar *value = NULL;
		if (!fu_firehose_device_read (device, &value, flags, error))
			return FALSE;

		if (value == NULL)
			return FALSE;

		if (g_strcmp0(value, "NAK") == 0 ||
			g_strcmp0(value, "ACK") == 0)
			break;

		if (g_str_has_prefix(value, "INFO: End of supported functions"))
			break;
	} while (1);
	return TRUE;
}

static gboolean
fu_firehose_command_power (FuDevice *device, const gchar **cmd, GError **error)
{
	*cmd = g_strdup(
		"<?xml version=\"1.0\" ?><data>"
		"<power value=\"reset\" /></data>");
	return TRUE;
}

static gboolean
fu_firehose_command_erase (XbNode *part, gchar **cmd, GError **error)
{
	const gchar *pages_per_block = xb_node_get_attr(part, "PAGES_PER_BLOCK");
	const gchar *sector_size_in_bytes = xb_node_get_attr(part, "SECTOR_SIZE_IN_BYTES");
	const gchar *num_partition_sectors = xb_node_get_attr(part, "num_partition_sectors");
	const gchar *physical_partition_number = xb_node_get_attr(part, "physical_partition_number");
	const gchar *start_sector = xb_node_get_attr(part, "start_sector");
	const gchar *last_sector = xb_node_get_attr(part, "last_sector");
	guint64 last_sector_num;

	g_return_val_if_fail (pages_per_block != NULL, FALSE);
	g_return_val_if_fail (sector_size_in_bytes != NULL, FALSE);
	g_return_val_if_fail (num_partition_sectors != NULL, FALSE);
	g_return_val_if_fail (physical_partition_number != NULL, FALSE);
	g_return_val_if_fail (start_sector != NULL, FALSE);

	if (!last_sector)
		last_sector_num = g_ascii_strtoull(start_sector, NULL, 0) + 
						  g_ascii_strtoull(num_partition_sectors, NULL, 0) - 1;
	else
		last_sector_num = g_ascii_strtoull(last_sector, NULL, 0);

	*cmd = g_strdup_printf (
		"<?xml version=\"1.0\" ?><data>"
		"<erase PAGES_PER_BLOCK=\"%s\" SECTOR_SIZE_IN_BYTES=\"%s\" last_sector=\"%lu\" "
		"num_partition_sectors=\"%s\" physical_partition_number=\"%s\" start_sector=\"%s\"/>"
		"</data>",
		pages_per_block,
		sector_size_in_bytes,
		last_sector_num,
		num_partition_sectors,
		physical_partition_number,
		start_sector);
	return TRUE;
}

/* fixup: the should should be the multiply of sector_size, append pad */
static guint
_fu_firehose_fixup_num_sectors(guint filesize, guint sector_size)
{
	guint num_sectors = filesize / sector_size;
	if (filesize % sector_size)
		num_sectors++;
	return num_sectors;
}

static gboolean
fu_firehose_command_program (XbNode *part, gchar **cmd, gsize filesize, GError **error)
{
	const gchar *pages_per_block = xb_node_get_attr(part, "PAGES_PER_BLOCK");
	const gchar *sector_size_in_bytes = xb_node_get_attr(part, "SECTOR_SIZE_IN_BYTES");
	const gchar *num_partition_sectors = xb_node_get_attr(part, "num_partition_sectors");
	const gchar *physical_partition_number = xb_node_get_attr(part, "physical_partition_number");
	const gchar *start_sector = xb_node_get_attr(part, "start_sector");
	const gchar *last_sector = xb_node_get_attr(part, "last_sector");
	guint sector_num;
	gsize sector_size;
	guint64 last_sector_num;

	g_return_val_if_fail (pages_per_block != NULL, FALSE);
	g_return_val_if_fail (sector_size_in_bytes != NULL, FALSE);
	g_return_val_if_fail (num_partition_sectors != NULL, FALSE);
	g_return_val_if_fail (physical_partition_number != NULL, FALSE);
	g_return_val_if_fail (start_sector != NULL, FALSE);

	sector_num = g_ascii_strtoull(num_partition_sectors, NULL, 0);
	sector_size = g_ascii_strtoull(sector_size_in_bytes, NULL, 0);

	/* fix up command */
	if (filesize)
		sector_num = _fu_firehose_fixup_num_sectors(filesize,
						sector_size);

	if (!last_sector)
		last_sector_num = g_ascii_strtoull(start_sector, NULL, 0) + 
						sector_num;
	else
		last_sector_num = g_ascii_strtoull(last_sector, NULL, 0);

	*cmd = g_strdup_printf (
		"<?xml version=\"1.0\" ?><data>"
		"<program PAGES_PER_BLOCK=\"%s\" SECTOR_SIZE_IN_BYTES=\"%s\" last_sector=\"%lu\" "
		"num_partition_sectors=\"%u\" physical_partition_number=\"%s\" start_sector=\"%s\"/>"
		"</data>",
		pages_per_block,
		sector_size_in_bytes,
		last_sector_num,
		sector_num,
		physical_partition_number,
		start_sector);
	return TRUE;
}

static void
fu_firehose_command_configure (FuDevice *device, gchar **cmd, GError **error)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	
	if (!cmd) {
		g_prefix_error (error, "invalid argument, expect not NULL");
		return;
	}

	*cmd = g_strdup_printf (
		"<?xml version=\"1.0\" ?><data>"
        "<configure MemoryName=\"%s\" MaxPayloadSizeFromTargetInBytes=\"%u\" "
        "AlwaysValidate=\"0\" MaxDigestTableSizeInBytes=\"2048\" MaxPayloadSizeToTargetInBytes=\"%u\" "
        "ZlpAwareHost=\"%d\" SkipStorageInit=\"%d\" />"
        "</data>",
        "nand", self->max_rx_size, self->max_tx_size, 0, 0); // only sdx20 support ZLP
}

static gboolean
_fu_firehose_get_absolute_path(XbNode *part, gchar **filename)
{
	gchar *fn = xb_node_get_attr(part, "filename");
	
	/* NULL or "" */
	if (!fn || strlen(fn) == 0)
		return FALSE;

	fn = g_strstrip(fn);
	if (rindex (fn, '\\'))
		*filename = g_strdup(rindex(fn, '\\') + 1);
	else
		*filename = g_strdup(fn);
	return TRUE;
}

static gboolean
fu_firehose_device_download (FuDevice *device,
					GBytes *fw,
					gsize totalsz,
					GError **error)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	gsize sz = g_bytes_get_size (fw);
	g_autoptr(GPtrArray) chunks = NULL;
	gsize padlen = totalsz - sz;
	g_autofree guint8 *tmp = NULL;
	g_autofree gchar *value = NULL;

	/* send the data in chunks */
	chunks = fu_chunk_array_new_from_bytes (fw,
						0x00,	/* start addr */
						0x00,	/* page_sz */
						self->max_tx_size);
	for (guint i = 0; i < chunks->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks, i);
		guint8 *data = chk->data;
		gsize datalen = chk->data_sz;

		if (i == chunks->len - 1 && padlen) {
			tmp = g_malloc0(chk->data_sz + padlen);
			memcpy (tmp, chk->data, chk->data_sz);
			data = tmp;
		    datalen = chk->data_sz + padlen;
		}

	LOGI("sending raw data %d/%d", chk->data_sz, totalsz);
		if (!fu_firehose_device_write (device, data, datalen, error))
			return FALSE;
		fu_device_set_progress_full (device, (gsize) i, (gsize) chunks->len * 2);
	}

	if (!fu_firehose_device_cmd(device, NULL,
			FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
			error))
		return FALSE;

	return TRUE;
}

static gboolean
fu_firehose_device_write_quectel_part (FuDevice *device,
					FuArchive *archive,
					XbNode *part,
					GError **error)
{
	const gchar *op = xb_node_get_element (part); //erase, program
	g_autofree gchar *tmp = NULL;

	/* erase */
	if (g_strcmp0 (op, "erase") == 0) {
		if (!fu_firehose_command_erase(part, &tmp, error))
			return FALSE;
		return fu_firehose_device_cmd (device, tmp,
						FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
						error);
	}

	/* flash */
	if (g_strcmp0 (op, "program") == 0) {
		g_autoptr(GPtrArray) chunks = NULL;
		g_autofree gchar *fn = NULL;
		GBytes *fw = NULL;
		gsize filesize = 0;
		guint sector_size = xb_node_get_attr_as_uint(part, "SECTOR_SIZE_IN_BYTES");
		guint num_sectors = xb_node_get_attr_as_uint(part, "num_partition_sectors");

		/* find filename */
		if (!_fu_firehose_get_absolute_path(part, &fn))
			return TRUE;

		fw = fu_archive_lookup_by_fn (archive, fn, error);
		if (fw == NULL)
			return FALSE;
		filesize = g_bytes_get_size (fw);

		if (!fu_firehose_command_program(part, &tmp, filesize, error))
			return FALSE;

		if (!fu_firehose_device_cmd (device, tmp,
						FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
						error))
			return FALSE;

		if (filesize)
			num_sectors = _fu_firehose_fixup_num_sectors(filesize, sector_size);
		return fu_firehose_device_download(device, fw, 
				num_sectors * sector_size, error);
	}

	/* unknown */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_INVALID_DATA,
		     "unknown operation %s", op);
	return FALSE;
}

static gboolean
fu_firehose_device_write_quectel (FuDevice *device, FuArchive* archive, GError **error)
{
	GBytes *data;
	g_autoptr(GPtrArray) erase_parts = NULL;
	g_autoptr(GPtrArray) program_parts = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *tmp = NULL;

	/* load the manifest of operations */
	data = fu_archive_lookup_by_fn_prefix (archive, FIREHOSE_XML_PREFIX, error);
	if (data == NULL)
		return FALSE;
	if (!xb_builder_source_load_bytes (source, data,
					   XB_BUILDER_SOURCE_FLAG_NONE, error))
		return FALSE;

	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, error);
	if (silo == NULL)
		return FALSE;

	/* when the prog_nand*.mbn runs, it will report some info
	 * in format of <log ..>
	 * including supportted functions
	 * 
	 * read them out or bulk transfer will be blocked
	 */
	if (!fu_firehose_device_cmd (device, NULL,
				FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
				error))
		return FALSE;

LOGI ("======try send configure");

	/* get all the data erase parts */
	fu_firehose_command_configure(device, &tmp, error);
	if (!fu_firehose_device_cmd (device, tmp,
					FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL,
					error))
		return FALSE;
		
LOGI ("======try erase/program");

	/* get all the data erase parts */
	erase_parts = xb_silo_query (silo, "data/erase", 0, error);
	if (erase_parts == NULL)
		return FALSE;

	for (guint i = 0; i < erase_parts->len; i++) {
		XbNode *part = g_ptr_array_index (erase_parts, i);
		if (!fu_firehose_device_write_quectel_part (device,
							     archive,
							     part,
							     error))
			return FALSE;
	}

	/* get all the data program parts */
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_WRITE);
	program_parts = xb_silo_query (silo, "data/program", 0, error);
	if (program_parts == NULL)
		return FALSE;

	for (guint i = 0; i < program_parts->len; i++) {
		XbNode *part = g_ptr_array_index (program_parts, i);
		if (!fu_firehose_device_write_quectel_part (device,
							     archive,
							     part,
							     error))
			return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_sahara_read (FuDevice *device,
			 guint8 *resp,
			 FuFirehoseDeviceReadFlags flags,
			 GError **error)
{
	// FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	GUsbDevice *usb_device = fu_usb_device_get_dev (FU_USB_DEVICE (device));
	guint retries = 1;

	/* these commands may return INFO or take some time to complete */
	if (flags & FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL)
		retries = FIREHOSE_TRANSACTION_RETRY_MAX;

	for (guint i = 0; i < retries; i++) {
		gboolean ret;
		gsize actual_len = 0;
		guint8 buf[MAX_RX_SIZE] = { 0x00 };
		g_autoptr(GError) error_local = NULL;

		ret = g_usb_device_bulk_transfer (usb_device,
						  FIREHOSE_EP_IN,
						  buf,
						  sizeof(buf),
						  &actual_len,
						  FIREHOSE_TRANSACTION_TIMEOUT,
						  NULL, &error_local);
		if (!ret) {
			if (g_error_matches (error_local,
					     G_USB_DEVICE_ERROR,
					     G_USB_DEVICE_ERROR_TIMED_OUT)) {
				LOGI ("ignoring %s", error_local->message);
				continue;
			}
			g_propagate_prefixed_error (error,
						    g_steal_pointer (&error_local),
						    "failed to do bulk in transfer: ");
			return FALSE;
		}
		fu_firehose_buffer_dump ("read", buf, actual_len);
		if (actual_len >= sizeof(sahara_common_header)) {
			if (resp != NULL)
				memcpy(resp, buf, actual_len);
			return TRUE;
		}

		/* unknown failure */
		g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "only read %" G_GSIZE_FORMAT "bytes", actual_len);
		return FALSE;
	}

	/* we timed out a *lot* */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "no response to read");
	return FALSE;
}

static gboolean
fu_sahara_hello_resp (FuDevice *device, sahara_mode mode, GError **error)
{
	gsize datalen = sizeof(sahara_hello_resp);
	g_autofree guint8 *data = g_malloc0(datalen);
	sahara_hello_resp *pkt = (sahara_hello_resp *)data;

	pkt->command = GINT32_TO_LE(SAHARA_HELLO_RESP);
	pkt->length = GINT32_TO_LE(sizeof(sahara_hello_resp));
	pkt->version = GINT32_TO_LE(SAHARA_VERSION);
	pkt->version_compatible = GINT32_TO_LE(SAHARA_VERSION_COMPATIBLE);
	pkt->status = GINT32_TO_LE(0);
	pkt->mode = GINT32_TO_LE(mode);

	return fu_firehose_device_write (device, data, datalen, error);
}

static gboolean
fu_sahara_done (FuDevice *device, GError **error)
{
	gsize datalen = sizeof(sahara_done);
	g_autofree guint8 *data = g_malloc0(datalen);
	sahara_done *pkt = (sahara_done *)data;

	pkt->command = GINT32_TO_LE(SAHARA_DONE);
	pkt->length = GINT32_TO_LE(sizeof(sahara_done));

	return fu_firehose_device_write (device, data, datalen, error);
}

static gboolean
fu_sahara_raw_data (FuDevice *device, GBytes *data, gsize offset, gsize datalen, GError **error)
{
	gsize size;
	const guint8 *raw_data = g_bytes_get_data(data, &size);
	
	raw_data += offset;
	return fu_firehose_device_write(device, raw_data, datalen, error);
}

static gboolean
fu_firehose_device_setup (FuDevice *device, GError **error)
{
	// FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	g_autofree gchar *product = NULL;
	g_autofree gchar *serialno = NULL;
	g_autofree gchar *version = NULL;
	g_autofree gchar *secure = NULL;
	g_autofree gchar *version_bootloader = NULL;

	/* product */
	// fu_device_set_name (device, "QHSUSB_BULK");

	/* boot-loader */
	fu_device_set_version_bootloader (device, "2.0.0");

	/* firmware version */
	fu_device_set_version(device, "1.2.2", FWUPD_VERSION_FORMAT_PLAIN);

	/* summary */
	fu_device_set_summary(device, "Qualcomm Modem in EDL mode");

	if (version_bootloader != NULL && version_bootloader[0] != '\0')
		fu_device_set_version_bootloader (device, version_bootloader);

	/* success */
	return TRUE;
}

static gboolean
fu_firehose_device_write_sahara (FuDevice *device, FuArchive* archive, GError **error)
{
	g_autofree guint8 *resp = g_malloc0 (MAX_RX_SIZE);
	GBytes *data = NULL;

	if (resp == NULL)
		return FALSE;

	/* load the manifest of operations */
	data = fu_archive_lookup_by_fn_prefix (archive, FIREHOSE_TOOL_PREFIX, error);
	if (data == NULL)
		return FALSE;

	do {
		sahara_common_header *hdr = (sahara_common_header*)resp;
		memset(resp, 0, MAX_RX_SIZE);
		if (!fu_sahara_read (device, resp, FU_FIREHOSE_DEVICE_READ_FLAG_STATUS_POLL, error))
			return FALSE;

        switch (GUINT32_FROM_LE(hdr->command)) {
        case SAHARA_HELLO:
		{
            if (!fu_sahara_hello_resp(device, SAHARA_MODE_IMAGE_TX_COMPLETE, error)) {
				g_prefix_error (error, "write sahara_hello_resp fail");
				return FALSE;
			}
            break;
        }
        case SAHARA_READ_DATA:
        {
            sahara_read_data *pkt = (sahara_read_data *)resp;
            if (!fu_sahara_raw_data(device, data, GUINT32_FROM_LE(pkt->offset),
					GUINT32_FROM_LE(pkt->datalen), error)) {
				g_prefix_error (error, "write sahara_raw_data fail");
				return FALSE;
            }
            break;
        }
        case SAHARA_END_IMG_TRANSFER:
        {
			sahara_end_img_transfer *pkt = (sahara_end_img_transfer *)resp;
            if (pkt->status) {
				g_prefix_error (error, "write sahara_end_img_tx fail %u", GUINT32_FROM_LE(pkt->status));
                return FALSE;
            }

            if (!fu_sahara_done(device, error)) {
				g_prefix_error (error, "write sahara_done fail");
                return FALSE;
            }
            break;
        }
        case SAHARA_DONE_RESP:
			/* success */
			g_debug ("sahara transfer finished");
			LOGI ("sahara transfer success");
			return TRUE;
        default:
			g_prefix_error (error, "read unknow sahara header");
			return FALSE;
        }
    } while (1);
}

static gboolean
fu_firehose_device_write_firmware (FuDevice *device,
				   FuFirmware *firmware,
				   FwupdInstallFlags flags,
				   GError **error)
{
	g_autoptr(FuArchive) archive = NULL;
	g_autoptr(GBytes) fw = NULL;

	/* get default image */
	fw = fu_firmware_get_image_default_bytes (firmware, error);
	if (fw == NULL)
		return FALSE;

	/* decompress entire archive ahead of time */
	archive = fu_archive_new (fw, FU_ARCHIVE_FLAG_IGNORE_PATH, error);
	if (archive == NULL)
		return FALSE;

	// /* load the prog_nand*.mbn of operations */
	if (fu_archive_lookup_by_fn_prefix (archive, FIREHOSE_TOOL_PREFIX, NULL) != NULL) {
		if (!fu_firehose_device_write_sahara (device, archive, error))
			return FALSE;
	}

	sleep(3);	
	/* load the manifest of operations */
	if (fu_archive_lookup_by_fn_prefix (archive, FIREHOSE_XML_PREFIX, NULL) != NULL) {
		if (!fu_firehose_device_write_quectel (device, archive, error))
			return FALSE;
	}

	/* not supported */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "manifest not supported");
	return FALSE;
}

static gboolean
fu_firehose_device_close (FuUsbDevice *device, GError **error)
{
	FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);
	GUsbDevice *usb_device = fu_usb_device_get_dev (device);

	/* we're done here */
	if (!g_usb_device_release_interface (usb_device, self->intf_nr,
					     G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER,
					     error)) {
		g_prefix_error (error, "failed to release interface: ");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_firehose_device_set_quirk_kv (FuDevice *device,
				 const gchar *key,
				 const gchar *value,
				 GError **error)
{
	// FuFirehoseDevice *self = FU_FIREHOSE_DEVICE (device);

	/* failed */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "quirk key not supported");
	return FALSE;

}

static gboolean
fu_firehose_device_attach (FuDevice *device, GError **error)
{
	g_autofree gchar *cmd = NULL;
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_RESTART);
	fu_firehose_command_power (device, &cmd, error))
	if (!fu_firehose_device_cmd (device, cmd,
				       FU_FIREHOSE_DEVICE_READ_FLAG_NONE,
				       error)) {
		fu_device_add_flag (device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	}
	else {
		fu_device_add_flag (device, FWUPD_DEVICE_FLAG_WAIT_FOR_REPLUG);
	}
	return TRUE;
}

static void
fu_firehose_device_init (FuFirehoseDevice *self)
{
	/* this is a safe default, even using USBv1 */
	self->max_tx_size = MAX_TX_SIZE;
	self->max_rx_size = MAX_RX_SIZE;
	self->intf_nr = 0;
	fu_device_set_protocol (FU_DEVICE (self), "com.qualcomm.firehose");
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_UPDATABLE);
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_IS_BOOTLOADER);
	fu_device_set_remove_delay (FU_DEVICE (self), FIREHOSE_REMOVE_DELAY_RE_ENUMERATE);
}

static void
fu_firehose_device_class_init (FuFirehoseDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	FuUsbDeviceClass *klass_usb_device = FU_USB_DEVICE_CLASS (klass);
	klass_device->probe = fu_firehose_device_probe;
	klass_device->setup = fu_firehose_device_setup;
	klass_device->write_firmware = fu_firehose_device_write_firmware;
	klass_device->attach = fu_firehose_device_attach;
	klass_device->to_string = fu_firehose_device_to_string;
	klass_device->set_quirk_kv = fu_firehose_device_set_quirk_kv;
	klass_usb_device->open = fu_firehose_device_open;
	klass_usb_device->close = fu_firehose_device_close;
}
