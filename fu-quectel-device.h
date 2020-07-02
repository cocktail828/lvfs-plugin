#pragma once

#include "fu-plugin.h"

#include <libusb-1.0/libusb.h>
#include "fu-quectel-firmware.h"

// appstream-util generate-guid "USB\VID_2C7C"
#define QUECTEL_COMMON_USB_GUID "22ae45db-f68e-5c55-9c02-4557dca238ec"
#define QUECTEL_USB_EC20CEFA_GUID "587bf468-6859-5522-93a7-6cce552a0aa3"

#define QUECTEL_USB_NORMAL_VID 0x2c7c
#define QUECTEL_USB_EDL_VID 0x05c6
#define QUECTEL_USB_EDL_PID 0x9008

typedef struct
{
	gchar *devpath;
	struct libusb_device_handle *handle;
	guint16 idVendor;
	guint16 idProduct;
	guint8 bNumInterfaces;
	gint16 wMaxPacketSize;
	guint8 ifno;
	guint8 ep_bulk_in;
	guint8 ep_bulk_out;
} QuectelUSBDev;

typedef enum
{
	STATE_INVALID,
	STATE_NORMAL,
	STATE_EDL,
} modem_state;

/**
 * APIs of usbfs
 */
modem_state fu_quectel_modem_state(QuectelUSBDev *usbdev);
QuectelUSBDev *fu_quectel_find_usb_device(void);
gboolean fu_quectel_open_usb_device(QuectelUSBDev *usbdev);
void fu_quectel_close_usb_device(QuectelUSBDev *usbdev);
gboolean fu_quectel_usb_device_send(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout);
gboolean fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout);
gboolean fu_quectel_usb_device_switch_mode(QuectelUSBDev *usbdev);
gchar *fu_quectel_get_version(QuectelUSBDev *usbdev);

gboolean fu_quectel_usb_device_sahara_write(QuectelUSBDev *usbdev, QuectelFirmware *fm);
gboolean fu_quectel_usb_device_firehose_write(QuectelUSBDev *usbdev, QuectelFirmware *fm);
void fu_quectel_usb_device_firehose_reset(QuectelUSBDev *usbdev);
