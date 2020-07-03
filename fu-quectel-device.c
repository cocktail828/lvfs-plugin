#include "fu-quectel-common.h"
#include "fu-quectel-device.h"

#include "protocol/sahara_proto.h"
#include "protocol/firehose_proto.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/usbdevice_fs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif
#include <sys/ioctl.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int max_bulk_rx_size = 4 * 1024;
int max_bulk_tx_size = FH_TX_MAX_SIZE;

static char *file_get_content(const char *fname, char *buffer, int want_read)
{
    FILE *fp = fopen(fname, "r");
    fscanf(fp, "%s", buffer);
    fclose(fp);

    return buffer;
}

int file_get_value(const char *fname, int base)
{
    int value;
    FILE *fp = fopen(fname, "r");
    if (!fp)
    {
        // LOGE("fail open " << fname << " " << strerror(errno));
        return -1;
    }

    if (base == 10)
        fscanf(fp, "%d", &value);
    else if (base == 16)
        fscanf(fp, "%x", &value);

    fclose(fp);
    return value;
}

static int fu_quectel_get_dm_interface(int vid, int pid)
{
    return 0;
}

void fu_quectel_usb_init(QuectelUSBDev *usbdev)
{
    libusb_init(NULL);
    memset(usbdev, 0, sizeof(QuectelUSBDev));
}

void fu_quectel_usb_deinit(QuectelUSBDev *usbdev)
{
    if (usbdev->handle)
        libusb_close(usbdev->handle);
}

bool fu_quectel_usb_arrival(struct libusb_device *dev)
{
    struct libusb_device_handle *handle;
    struct libusb_device_descriptor desc;
    int ret;

    libusb_get_device_descriptor(dev, &desc);
    LOGI("Add usb device: %04x:%04x", desc.idVendor, desc.idProduct);
}

bool fu_quectel_usb_left(struct libusb_device *dev)
{
    struct libusb_device_handle *handle;
    struct libusb_device_descriptor desc;
    int ret;

    libusb_get_device_descriptor(dev, &desc);
    LOGI("Remove usb device: %04x:%04x", desc.idVendor, desc.idProduct);
}

/**
 * set endpoint info
 */
static void quectel_usb_set_info(QuectelUSBDev *usbdev,
                                 struct libusb_device *dev)
{
    struct libusb_config_descriptor *config = NULL;
    const struct libusb_interface *iface = NULL;
    int ifidx = fu_quectel_get_dm_interface(usbdev->idVendor, usbdev->idProduct);
    LINE
        libusb_get_active_config_descriptor(dev, &config);
    usbdev->bNumInterfaces = config->bNumInterfaces;

    if (config->bNumInterfaces <= ifidx)
    {
        libusb_free_config_descriptor(config);
        return;
    }

    iface = &config->interface[ifidx];
    for (int altsetting_index = 0; altsetting_index < iface->num_altsetting; altsetting_index++)
    {
        const struct libusb_interface_descriptor *altsetting = &iface->altsetting[altsetting_index];
        for (int endpoint_index = 0; endpoint_index < altsetting->bNumEndpoints; endpoint_index++)
        {
            const struct libusb_endpoint_descriptor *endpoint = &altsetting->endpoint[endpoint_index];
            if (endpoint->bEndpointAddress & 0x80)
                usbdev->ep_bulk_in = endpoint->bEndpointAddress;

            if (!(endpoint->bEndpointAddress & 0x80))
                usbdev->ep_bulk_out = endpoint->bEndpointAddress;
            usbdev->wMaxPacketSize = endpoint->wMaxPacketSize;
        }
    }
    libusb_free_config_descriptor(config);
}

/**
 * notice:
 * most of Quectel modem (Qualcomm chipset inside) will cone up with vendor id 0x05c6 product id 0x9008 
 * if stay in upgrade mode using firehose down protocol.
 */
modem_state fu_quectel_scan_usb_device(QuectelUSBDev *usbdev)
{
    int cnt;
    struct libusb_device **list;
    modem_state state = STATE_INVALID;

    cnt = libusb_get_device_list(NULL, &list);
    for (int i = 0; i < cnt; i++)
    {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(list[i], &desc);

        usbdev->idVendor = desc.idVendor;
        usbdev->idProduct = desc.idProduct;
        quectel_usb_set_info(usbdev, list[i]);
        if (desc.idVendor == QUECTEL_USB_NORMAL_VID)
        {
            state = STATE_NORMAL;
            break;
        }

        if (desc.idVendor == QUECTEL_USB_EDL_VID && desc.idProduct == QUECTEL_USB_EDL_PID)
        {
            state = STATE_EDL;
            break;
        }
    }
    libusb_free_device_list(list, 1);

    return state;
}

bool fu_quectel_open_usb_device(QuectelUSBDev *usbdev)
{
    int ret;
    libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, usbdev->idVendor, usbdev->idProduct);
    if (!handle)
    {
        LOGE("fail open device %04x:%04x", usbdev->idVendor, usbdev->idProduct);
        return false;
    }

    libusb_reset_device(handle);
    libusb_set_auto_detach_kernel_driver(handle, true);
    int ifidx = fu_quectel_get_dm_interface(usbdev->idVendor, usbdev->idProduct);
    ret = libusb_claim_interface(handle, ifidx);
    if (ret)
    {
        LOGE("fail claim usb %04x:%04x ifidx %d, %s", usbdev->idVendor, usbdev->idProduct, ifidx, libusb_strerror(ret));
        return false;
    }
    usbdev->handle = handle;

    return true;
}

int fu_quectel_usb_device_send(QuectelUSBDev *usbdev, uint8_t *data, int datalen)
{
    int transfered = 0;

    do
    {
        int actlen;
        int tx_size = (datalen > max_bulk_tx_size) ? max_bulk_tx_size : datalen;
        int ret = libusb_bulk_transfer(usbdev->handle, usbdev->ep_bulk_out, data, tx_size, &actlen, TIMEOUT);
        if (!ret && actlen == tx_size)
        {
            transfered += actlen;
            datalen -= tx_size;
        }
        else
        {
            LOGE("bulk tx error (%d/%d/%d), %s", actlen, tx_size, transfered, libusb_strerror(ret));
            break;
        }

        if (datalen == 0)
            break;
    } while (1);

    return transfered;
}

int fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, uint8_t *data, int datalen)
{
    int actlen;
    int ret = libusb_bulk_transfer(usbdev->handle, usbdev->ep_bulk_in, data, datalen, &actlen, TIMEOUT);
    if (ret)
        LOGE("bulk rx error, rx=%d", actlen);

    return actlen;
}

bool fu_quectel_usb_device_switch_mode(QuectelUSBDev *usbdev)
{
    int max_try = 5;
    uint8_t edl_command[] = {0x4b, 0x65, 0x01, 0x00, 0x54, 0x0f, 0x7e};
    modem_state state = fu_quectel_scan_usb_device(usbdev);

    while (state != STATE_EDL && max_try)
    {
        fu_quectel_usb_device_send(usbdev, edl_command, array_len(edl_command));
        usleep(3);
        max_try--;
        if (!max_try)
            break;
        state = fu_quectel_scan_usb_device(usbdev);
    }

    return (state == STATE_EDL);
}

char *fu_quectel_get_version(QuectelUSBDev *usbdev)
{
    return "1.2.0";
}

bool fu_quectel_usb_device_sahara_write(QuectelUSBDev *usbdev, const char *prog)
{
    bool status;
    uint8_t buffer[SAHARA_MAX_PKT]; // recv buffer
    int imgfd;

    imgfd = open(prog, O_RDONLY);
    do
    {
        if (fu_quectel_usb_device_recv(usbdev, buffer, array_len(buffer)))
        {
            LOGE("sahara recv command fail");
            status = false;
            goto quit;
        }

        switch (sahara_command_type(buffer))
        {
        case SAHARA_HELLO:
        {
            LOGI("<- sahara_hello");
            if (fu_quectel_usb_device_send(usbdev,
                                           (uint8_t *)new_sahara_hello_resp(SAHARA_MODE_IMAGE_TX_COMPLETE),
                                           sizeof(sahara_hello_resp)))
            {
                LOGE("sahara hello response error");
                status = false;
                goto quit;
            }
            LOGI("-> sahara_hello_resp");
            break;
        }
        case SAHARA_READ_DATA:
        {
            sahara_read_data *pkt = (sahara_read_data *)buffer;
            LOGI("<- sahara_read_data offset %u len %u", le32toh(pkt->offset), le32toh(pkt->datalen));

            if (fu_quectel_usb_device_send(usbdev,
                                           (uint8_t *)new_sahara_raw_data(imgfd, le32toh(pkt->offset), le32toh(pkt->datalen)),
                                           le32toh(pkt->datalen)))
            {
                LOGI("sahara_send_cmd error");
                status = false;
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
                status = false;
                goto quit;
            }

            if (fu_quectel_usb_device_send(usbdev,
                                           (uint8_t *)new_sahara_done(),
                                           sizeof(sahara_done)))
            {
                LOGI("sahara_send_cmd error");
                status = false;
                goto quit;
            }
            LOGI("-> sahara_done");
            break;
        }
        case SAHARA_DONE_RESP:
        {
            LOGI("<- sahara_done_resp");
            LOGI("image has been successfully transfered");
            status = true;
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

static int
get_file_size(int fd)
{
    struct stat statbuf;
    fstat(fd, &statbuf);
    return statbuf.st_size;
}

static bool
fu_quectel_send_raw_data(QuectelUSBDev *usbdev, const char *img, int sector_size)
{
    bool status;
    uint8_t buffer[FH_TX_MAX_SIZE];
    int imgfd = open(img, O_RDONLY);
    int remain_len = get_file_size(imgfd);

    do
    {
        int tx_size = (remain_len > FH_TX_MAX_SIZE) ? FH_TX_MAX_SIZE : sector_size;
        memset(buffer, 0, array_len(buffer));
        read(imgfd, buffer, tx_size);
        status = fu_quectel_usb_device_send(usbdev, buffer, tx_size);
        remain_len -= tx_size;
    } while (remain_len);
    return status;
}

bool fu_quectel_usb_device_firehose_write(QuectelUSBDev *usbdev, QuectelFirmware *fm)
{
    char *cmd = NULL;
    bool status;

    // for (int i = 0; fm[i]; i++)
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

    //     status = fu_quectel_usb_device_send(usbdev, cmd, strlen(cmd));
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
    char *cmd = new_firehose_power("reset");
    fu_quectel_usb_device_send(usbdev, (uint8_t *)cmd, strlen(cmd));
}