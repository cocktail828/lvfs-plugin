#include "fu-quectel-common.h"
#include "fu-quectel-device.h"

#include "protocol/sahara_proto.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static guint fu_quectel_dm_interface_num(guint16 vid, guint16 pid)
{
    return 0;
}

static void fu_quectel_get_usb_devpath(QuectelUSBDev *usbdev)
{
    int ret;
    static gchar devpath[64];
    uint8_t port_num[32];

    usbdev->devpath = devpath;
    devpath[0] = '\0';
    ret = libusb_get_port_numbers(usbdev->handle, port_num, array_len(port_num));
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
    fu_quectel_find_usb_device();
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

gboolean
fu_quectel_usb_device_send(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout)
{
    return TRUE;
}

gboolean
fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, guint8 *buffer, guint datalen, guint timeout)
{
    return TRUE;
}

gboolean
fu_quectel_usb_device_switch_mode(QuectelUSBDev *usbdev)
{
    guint max_try = 5;
    guint8 edl_command[] = {0x4b, 0x65, 0x01, 0x00, 0x54, 0x0f, 0x7e};

    while (fu_quectel_modem_state(usbdev) != STATE_EDL)
    {
        fu_quectel_usb_device_send(usbdev, edl_command, array_len(edl_command), TIMEOUT);
        usleep(3);
    }

    return (fu_quectel_modem_state(usbdev) == STATE_EDL);
}

gchar *fu_quectel_get_version(QuectelUSBDev *usbdev)
{
    return "1.2.0";
}

gboolean
fu_quectel_usb_device_sahara_write(QuectelUSBDev *usbdev, const gchar *prog)
{
    gboolean status;
    guint8 buffer[SAHARA_MAX_PKT]; // recv buffer
    int imgfd;

    imgfd = open(prog, O_RDONLY);
    do
    {
        if (fu_quectel_usb_device_recv(usbdev, buffer, array_len(buffer), TIMEOUT))
        {
            LOGE("sahara recv command fail");
            status = FALSE;
            goto quit;
        }

        switch (sahara_command_type(buffer))
        {
        case SAHARA_HELLO:
        {
            LOGI("<- sahara_hello");
            if (fu_quectel_usb_device_send(usbdev,
                                           new_sahara_hello_resp(SAHARA_MODE_IMAGE_TX_COMPLETE),
                                           sizeof(sahara_hello_resp), TIMEOUT))
            {
                LOGE("sahara hello response error");
                status = FALSE;
                goto quit;
            }
            LOGI("-> sahara_hello_resp");
            break;
        }
        case SAHARA_READ_DATA:
        {
            sahara_read_data *pkt = (sahara_common_header *)buffer;
            LOGI("<- sahara_read_data offset %u len %u", le32toh(pkt->offset), le32toh(pkt->datalen));

            if (fu_quectel_usb_device_send(usbdev,
                                           new_sahara_raw_data(imgfd, le32toh(pkt->offset), le32toh(pkt->datalen)),
                                           le32toh(pkt->datalen), TIMEOUT))
            {
                LOGI("sahara_send_cmd error");
                status = FALSE;
                goto quit;
            }
            LOGI("-> sahara_raw_data");
            break;
        }
        case SAHARA_END_IMG_TRANSFER:
        {
            LOGI("<- sahara_end_image_transfer");
            if (sahara_tx_finish((sahara_end_img_transfer *)buffer))
            {
                LOGI("target send SAHARA_END_IMG_TRANSFER with error");
                status = FALSE;
                goto quit;
            }

            if (fu_quectel_usb_device_send(usbdev,
                                           new_sahara_done(),
                                           sizeof(sahara_done), TIMEOUT))
            {
                LOGI("sahara_send_cmd error");
                status = FALSE;
                goto quit;
            }
            LOGI("-> sahara_done");
            break;
        }
        case SAHARA_DONE_RESP:
        {
            LOGI("<- sahara_done_resp");
            LOGI("image has been successfully transfered");
            status = TRUE;
            goto quit;
        }
        default:
            LOGI("invalid sahara command recevied");
        }
    } while (1);

quit:
    close(imgfd);
    return status;
}

guint get_file_size(int fd)
{
    struct stat statbuf;
    fstat(fd, &statbuf);
    return statbuf.st_size;
}

gboolean
fu_quectel_send_raw_data(QuectelUSBDev *usbdev, const gchar *img, guint sector_size)
{
    gboolean status;
    gchar buffer[FH_TX_MAX];
    int imgfd = open(img, O_RDONLY);
    guint remain_len = get_file_size(imgfd);

    do
    {
        memset(buffer, 0, array_len(buffer));
        guint tx_size = (remain_len > FH_TX_MAX) ? FH_TX_MAX : sector_size;
        read(imgfd, buffer, tx_size);
        status = fu_quectel_usb_device_send(usbdev, buffer, tx_size, TIMEOUT);
        remain_len -= tx_size;
    } while (remain_len);
    return status;
}

gboolean fu_quectel_usb_device_firehose_write(QuectelUSBDev *usbdev, QuectelFirmware *fm)
{
    gchar *cmd = NULL;
    gboolean status;

    // for (guint i = 0; fm[i]; i++)
    // {
    //     QuectelFirmware *ptr = fm[i];
    //     switch (ptr->type)
    //     {
    //     case CMD_ERASE:
    //         cmd = new_firehose_erase(ptr->data);
    //         break;
    //     case CMD_PROGRAM:
    //         cmd = new_firehose_program(ptr->data);
    //     default:
    //         break;
    //     }

    //     status = fu_quectel_usb_device_send(usbdev, cmd, strlen(cmd), TIMEOUT);
    //     if (!status)
    //     {
    //         LOGE("fail do firehose command");
    //         break;
    //     }

    //     // program command, send raw data
    //     if (ptr->type == CMD_PROGRAM)
    //     {
    //         status = fu_quectel_send_raw_data(usbdev, ptr->img, ptr->sector_size);
    //         if (!status)
    //         {
    //             LOGE("fail send raw data");
    //             break;
    //         }
    //     }
    // }

    return status;
}

void fu_quectel_usb_device_firehose_reset(QuectelUSBDev *usbdev)
{
    gchar *cmd = new_firehose_power("reset");
    fu_quectel_usb_device_send(usbdev, cmd, strlen(cmd), TIMEOUT);
}
