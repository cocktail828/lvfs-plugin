#pragma once

// #include "fu-plugin.h"
#include "fu-quectel-firmware.h"

#include <stdint.h>
#include <libusb-1.0/libusb.h>

// appstream-util generate-guid "USB\VID_2C7C"
#define QUECTEL_COMMON_USB_GUID "22ae45db-f68e-5c55-9c02-4557dca238ec"
#define QUECTEL_USB_EC20CEFA_GUID "587bf468-6859-5522-93a7-6cce552a0aa3"

#define QUECTEL_USB_NORMAL_VID 0x2c7c
#define QUECTEL_USB_EDL_VID 0x05c6
#define QUECTEL_USB_EDL_PID 0x9008

typedef struct
{
	char devpath[32];
	void *handle;
	uint16_t idVendor;
	uint16_t idProduct;

	uint8_t bNumInterfaces;
	uint16_t wMaxPacketSize;
	uint8_t ifidx;
	uint8_t ep_bulk_in;
	uint8_t ep_bulk_out;
} QuectelUSBDev;

typedef enum
{
	STATE_INVALID,
	STATE_NORMAL,
	STATE_EDL,
} modem_state;

#define TIMEOUT 5000
#define FH_TX_MAX_SIZE (16 * 1024)

void fu_quectel_usb_init(QuectelUSBDev *usbdev);

void fu_quectel_usb_deinit(QuectelUSBDev *usbdev);

/**
 * try to find device; notice, there should be only one quectel device
 *  usb:
 *      nornal mode 2c7c
 *      edl mode 05c6:9008
 *  pci:
 *      todo in the fulture
 */
modem_state fu_quectel_scan_usb_device(QuectelUSBDev *usbdev);

bool fu_quectel_open_usb_device(QuectelUSBDev *usbdev);
void fu_quectel_close_usb_device(QuectelUSBDev *usbdev);
int fu_quectel_usb_device_send(QuectelUSBDev *usbdev, uint8_t *data, int datalen);
int fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, uint8_t *data, int datalen);
bool fu_quectel_usb_device_switch_mode(QuectelUSBDev *usbdev);
char *fu_quectel_get_version(QuectelUSBDev *usbdev);

/**
 * transfer prog_nand.mbn(firehose flash tool) into modem
 */
bool fu_quectel_usb_device_sahara_write(QuectelUSBDev *usbdev, const char *prog);

/**
 * do operations via firehose protocol
 */
bool fu_quectel_usb_device_firehose_write(QuectelUSBDev *usbdev, QuectelFirmware *fm);

/**
 * do reset via firehose protocol
 */
void fu_quectel_usb_device_firehose_reset(QuectelUSBDev *usbdev);
