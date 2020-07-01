/*
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"
#include "fu-quectel-common.h"
#include "errno.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
// #include "appstream-glib.h"

QDev dev_list[] = {
	{0x2c7c, 0x0125},
	{0x0000, 0x0000},
};

gchar *tty_dev = NULL;

/* dump at command, ignore response */
gboolean
fu_quectel_command_send(int ttyhandler, CMDType type, gchar *cmd) {
	int ret;

	g_return_val_if_fail(ttyhandler != -1, FALSE);
	if (type == CMD_AT) {
		ret = write(ttyhandler, cmd, strlen(cmd));
		if (ret == -1) {
			g_printerr("Failed to dump command %s ret = %d\r\n", cmd, ret);
			return FALSE;
		}else {
			g_print("Succeed to send command %s", cmd);
		}
	}

	if (type == CMD_FB) {
		ret = system(cmd);
		if (ret == -1) {
			g_printerr("System error ret = %d!\r\n", ret);
			return FALSE;
		}else {
			if (WIFEXITED(ret)){
				if (WEXITSTATUS(ret) == 0) {
					return TRUE;
				}
				g_printerr("System run %s error, ret = %d\r\n", cmd, WEXITSTATUS(ret));
			}
			g_printerr("System error: child exit code = %d\r\n", WEXITSTATUS(ret));
		}
		return FALSE;
	}
	return TRUE;
}

/* dump at command, get response */
int
fu_quectel_command_resp(int ttyhandler, gchar *resp, int size) {
	int ret;

	g_return_val_if_fail(ttyhandler != -1, -1);
	do {
		ret = read(ttyhandler, resp, size);
		if (ret < 0) {
			g_printerr("Get resp ret = %d errno = %d(%s)\r\n", ret, errno, strerror(errno));
			if (errno == EAGAIN) {
				// sleep(1);
				continue;
			} else {
				return -1;
			}
		}else{
			return ret;
		}
	} while (1);
	return ret;
}

void
fu_quectel_release_ttyhandler(int handler) {
	g_return_if_fail(handler != -1);
	close(handler);
}

void
fu_quectel_set_ttyhandler(const gchar *path, const gchar *name) {
	g_autofree gchar *subname = NULL;
	g_autofree gchar *prefix = NULL;
	g_autofree gchar *tmpname = NULL;
	g_autofree gchar *interface_id = NULL;
	g_autoptr(GDir) pdir = NULL;

	prefix = g_strdup_printf("%s/%s/%s:", path, name, name);

	pdir = g_dir_open(g_strdup_printf("%s/%s", path, name), 0, NULL);
	g_return_if_fail(pdir != NULL);
	while ((subname = g_dir_read_name(pdir)) != NULL) {
		tmpname = g_strdup_printf("%s/%s/%s/bInterfaceNumber", path, name, subname);
		if (g_file_test(tmpname, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) &&
			g_str_has_prefix(tmpname, prefix)) {
			g_file_get_contents(tmpname, &interface_id, NULL, NULL);
			if (strtoul(interface_id, NULL, 10) % 5 == 2 && interface_id != NULL) {
				tty_dev = g_strdup_printf("/dev/ttyUSB%lu", strtoul(interface_id, NULL, 10));
				// ttyhandler = open(tty_dev, O_NOCTTY|O_NONBLOCK|O_RDWR);
				if (tty_dev != NULL) {
					g_print("Found quectel tty device %s\r\n", tty_dev);
				} else {
					g_printerr("Fail to open tty device %s\r\n", tty_dev);
				}
			}
		}
	}
}

gboolean
fu_quectel_check_vid_pid(const gchar *path, const guint32 vid, const guint32 pid) {
	g_autoptr(GDir) pdir = NULL;
    const gchar *name = NULL;
	g_autofree gchar *tmp_vid = NULL;
	g_autofree gchar *tmp_pid = NULL;
	g_autofree gchar *fvid = NULL;
	g_autofree gchar *fpid = NULL;

	/* /sys/bus/usb/devices/usbx */
	pdir = g_dir_open(path, 0, NULL);
	g_return_val_if_fail(pdir != NULL, FALSE);
	while ((name = g_dir_read_name(pdir)) != NULL) {
		fvid = g_strdup_printf("%s/%s/idVendor", path, name);
		fpid = g_strdup_printf("%s/%s/idProduct", path, name);

		if (g_file_test(fvid, G_FILE_TEST_EXISTS) && g_file_test(fvid, G_FILE_TEST_EXISTS)) {
			fu_quectel_set_ttyhandler(path, name);
			g_file_get_contents(fvid, &tmp_vid, NULL, NULL);
			g_file_get_contents(fpid, &tmp_pid, NULL, NULL);
			if (strtoul(tmp_vid, NULL, 16) == vid && strtoul(tmp_pid, NULL, 16) == pid) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

gboolean
fu_quectel_get_product(guint32 vid, guint32 pid, gchar **product) {
	g_autoptr(GDir) pdir = NULL;
	const gchar *root = "/sys/bus/usb/devices";
	const gchar *name = NULL;
	g_autofree gchar *fname = NULL;
	g_autofree gchar *path = NULL;

	/* /sys/bus/usb/devices/usbx */
	pdir = g_dir_open(root, 0, NULL);
	g_return_val_if_fail(pdir != NULL, FALSE);
	while ((name = g_dir_read_name(pdir)) != NULL) {
		if (! g_str_has_prefix(name, "usb")) {
			continue;
		}

		/* /sys/bus/usb/devices/usbx */
		path = g_strdup_printf("%s/%s", root, name);
		if (fu_quectel_check_vid_pid(path, vid, pid)) {
			fname = g_strdup_printf("%s/product", path);
			if (g_file_test(fname, G_FILE_TEST_EXISTS)) {
				/* get product desp info /sys/bus/usb/devices/usbx/product */
				g_file_get_contents(fname, product, NULL, NULL);
				g_strstrip(*product);
				return TRUE;
			}
		} 
	}

	return FALSE;
}

gchar*
fu_quectel_calc_guid(guint32 vid, guint32 pid) {
	g_autofree gchar *rawstr = NULL;
	g_autofree gchar *product = NULL;
	gboolean ret;

	ret = fu_quectel_get_product(vid, pid, &product);
	g_return_val_if_fail(ret == TRUE, NULL);
	/* vid + pid + product */
	// rawstr = g_strdup_printf("USB\\VID_%04x&PID_%04x&PRO_%s", vid, pid, product);
	g_print("Guid: %s\r\n", rawstr);
	// return as_utils_guid_from_string (rawstr);
	return "";
}

gboolean
fu_quectel_is_my_dev(guint32 vid, guint32 pid) {
	guint32 idx = 0;
	while (dev_list[idx].vid && dev_list[idx].pid) {
		if (dev_list[idx].vid  == vid && dev_list[idx].pid == pid) {
			return TRUE;
		}
		idx++;
	}

	if (vid == QUECTEL_VID) {
		return TRUE;
	}
	return FALSE;
}