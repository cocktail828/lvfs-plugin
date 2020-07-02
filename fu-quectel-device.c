#include "fu-quectel-common.h"
#include "fu-quectel-device.h"

static guint fu_quectel_dm_interface_num(guint16 vid, guint16 pid)
{
    return 0;
}

static void fu_quectel_get_usb_devpath(QuectelUSBDev *usbdev)
{
    int ret ;
    static gchar devpath[64];
    uint8_t port_num[32];

    usbdev->devpath = devpath;
    devpath[0] = '\0';
ret= libusb_get_port_numbers(usbdev->handle, port_num, array_len(port_num));
    for (int i = 0; i < ret; i++)
        g_snprintf(devpath, strlen(devpath), ":%d", port_num[i]);
}

/**
 * try to find device
 *  usb:
 *      nornal mode 2c7c
 *      edl mode 05c6:9008
 *  pci:
 *      todo in the fulture
 */
QuectelUSBDev *fu_quectel_find_usb_device(void)
{
    struct libusb_device **devs = NULL;
    struct libusb_device_handle *handle = NULL;
    struct libusb_device *dev = NULL;
    static QuectelUSBDev usbdev; // = g_malloc0(sizeof(QuectelUSBDev));

    int ret = libusb_init(NULL);
    libusb_get_device_list(NULL, &devs);

    for (guint i = 0; devs[i] != NULL; i++)
    {
        const struct libusb_interface *iface = NULL;
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *config = NULL;
        guint ifnum;

        dev = devs[i];
        ret = libusb_get_device_descriptor(dev, &desc);
        if (ret < 0)
        {
            LOGE("failed to get device descriptor");
            goto error;
        }

        if (desc.idVendor != QUECTEL_USB_NORMAL_VID &&
            !(desc.idVendor == QUECTEL_USB_EDL_VID && desc.idProduct == QUECTEL_USB_EDL_PID))
            continue;

        usbdev.idVendor = desc.idVendor;
        usbdev.idProduct = desc.idProduct;
        ret = libusb_open(dev, &handle);
        if (ret)
        {
            LOGE("error opening device %s", libusb_strerror(ret));
            goto error;
        }

        libusb_get_active_config_descriptor(dev, &config);
        usbdev.bNumInterfaces = config->bNumInterfaces;
        ifnum = fu_quectel_dm_interface_num(desc.idVendor, desc.iProduct);
        if (ifnum >= config->bNumInterfaces)
        {
            goto error;
        }

        iface = &config->interface[ifnum];
        for (int altsetting_index = 0; altsetting_index < iface->num_altsetting; altsetting_index++)
        {
            const struct libusb_interface_descriptor *altsetting = &iface->altsetting[altsetting_index];
            for (int endpoint_index = 0; endpoint_index < altsetting->bNumEndpoints; endpoint_index++)
            {
                const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[endpoint_index];
                if (endpoint->bEndpointAddress & 0x80)
                    usbdev.ep_bulk_in = endpoint->bEndpointAddress;
                else
                    usbdev.ep_bulk_out = endpoint->bEndpointAddress;
                usbdev.wMaxPacketSize = endpoint->wMaxPacketSize;
            }
        }

        libusb_free_config_descriptor(config);
    }

    LINE
        usbdev.handle = handle;
    fu_quectel_get_usb_devpath(&usbdev);
    libusb_free_device_list(devs, 1);
    libusb_close(handle);
    return &usbdev;

error:
    LINE
        libusb_free_device_list(devs, 1);
    libusb_close(handle);
    // g_free(usbdev);
    return NULL;
}

/**
 * notice:
 * most of Quectel modem (Qualcomm chipset inside) will cone up with vendor id 0x05c6 product id 0x9008 
 * if stay in upgrade mode using firehose down protocol.
 */
modem_state fu_quectel_modem_state(QuectelUSBDev *usbdev)
{
    if (usbdev->idVendor == QUECTEL_USB_NORMAL_VID)
        return STATE_NORMAL;

    if (usbdev->idVendor == QUECTEL_USB_EDL_VID && usbdev->idProduct == QUECTEL_USB_EDL_PID)
        return STATE_EDL;

    return STATE_INVALID;
}

gboolean fu_quectel_open_usb_device(QuectelUSBDev *usbdev)
{
    int ret;
    guint ifno = usbdev->ifno;
    libusb_device_handle *handle = usbdev->handle;

    libusb_set_auto_detach_kernel_driver(handle, TRUE);
    ret = libusb_claim_interface(handle, ifno);
    if (ret)
    {
        LOGE("fail to claim interface %u, %s", ifno, libusb_strerror(ret));
        return FALSE;
    }
    return TRUE;
}

void fu_quectel_close_usb_device(QuectelUSBDev *usbdev)
{
    libusb_close(usbdev->handle);
}

gboolean fu_quectel_usb_device_send(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout)
{
    return TRUE;
}

gboolean fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout)
{
    return TRUE;
}

gchar *fu_quectel_get_version(QuectelUSBDev *usbdev)
{
    return "1.2.0";
}
