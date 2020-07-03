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
int max_bulk_tx_size = 16 * 1024;

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

static int fu_quectel_dm_interface_num(int vid, int pid)
{
    return 0;
}

void fu_quectel_set_endpoint_info(QuectelUSBDev *usbdev, const char *parent)
{
    char devpath[512];
    struct dirent *entptr = NULL;
    DIR *dirptr = NULL;

    snprintf(devpath, array_len(devpath), "%s/devpath", parent);
    file_get_content(devpath, usbdev->devpath, array_len(usbdev->devpath));

    usbdev->ifno = fu_quectel_dm_interface_num(usbdev->idVendor, usbdev->idProduct);

    snprintf(devpath, array_len(devpath), "%s", parent);
    dirptr = opendir(devpath);
    if (!dirptr)
        return;

    while ((entptr = readdir(dirptr)))
    {
        struct dirent *subentptr = NULL;
        DIR *subdirptr = NULL;

        if (entptr->d_name[0] == '.')
            continue;

        snprintf(devpath, array_len(devpath), "%s/%s/bInterfaceNumber", parent, entptr->d_name);
        if (usbdev->ifno != file_get_value(devpath, 10))
            continue;

        snprintf(devpath, array_len(devpath), "%s/%s", parent, entptr->d_name);
        subdirptr = opendir(devpath);
        if (!subdirptr)
            break;
        while ((subentptr = readdir(subdirptr)))
        {
            int epaddr;
            if (subentptr->d_name[0] == '.' || strncasecmp(subentptr->d_name, "ep_", 3))
                continue;

            snprintf(devpath, array_len(devpath), "%s/%s/%s/wMaxPacketSize", parent, entptr->d_name, subentptr->d_name);
            usbdev->wMaxPacketSize = file_get_value(devpath, 10);

            epaddr = strtoul(((char *)subentptr->d_name + 3), NULL, 16);
            if (epaddr & 0x80)
                usbdev->ep_bulk_in = epaddr;
            else
                usbdev->ep_bulk_out = epaddr;
        }
        closedir(subdirptr);
    }
}

bool fu_quectel_scan_usb_device(QuectelUSBDev *usbdev)
{
    DIR *dirptr = NULL;
    struct dirent *entptr = NULL;
    char devpath[512];
    bool found = false;

    snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/");
    dirptr = opendir(devpath);
    if (!dirptr)
    {
        LOGE("fail to open %s", devpath);
        return false;
    }

    while ((entptr = readdir(dirptr)))
    {
        int vid, pid;
        if (entptr->d_name[0] == '.')
            continue;

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/idVendor", entptr->d_name);
        vid = file_get_value(devpath, 16);

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/idProduct", entptr->d_name);
        pid = file_get_value(devpath, 16);

        if (!(vid == QUECTEL_USB_NORMAL_VID ||
              (vid == QUECTEL_USB_EDL_VID && pid == QUECTEL_USB_EDL_PID)))
            continue;

        usbdev->idVendor = vid;
        usbdev->idProduct = pid;

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/busnum", entptr->d_name);
        usbdev->busnum = file_get_value(devpath, 10);

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/devnum", entptr->d_name);
        usbdev->devnum = file_get_value(devpath, 10);

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/bNumInterfaces", entptr->d_name);
        usbdev->bNumInterfaces = file_get_value(devpath, 10);

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s", entptr->d_name);
        fu_quectel_set_endpoint_info(usbdev, devpath);

        found = true;
        break;
    }
    closedir(dirptr);

    snprintf(devpath, array_len(devpath), "/dev/bus/usb/%03d/%03d", usbdev->busnum, usbdev->devnum);
    LOGI("find device %s", devpath);

    LOGI("%s %x %x bus %d dev %d numif %d maxpkt %d ifno %d in %x out %x",
         usbdev->devpath,
         (int)usbdev->idVendor,
         (int)usbdev->idProduct,
         (int)usbdev->busnum,
         (int)usbdev->devnum,
         (int)usbdev->bNumInterfaces,
         (int)usbdev->wMaxPacketSize,
         (int)usbdev->ifno,
         (int)usbdev->ep_bulk_in,
         (int)usbdev->ep_bulk_out);

    return found;
}

bool get_devpath_from_vid_pid(QuectelUSBDev *usbdev)
{
    DIR *dirptr = NULL;
    struct dirent *entptr = NULL;
    char devpath[512];
    bool found = false;

    snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/");
    dirptr = opendir(devpath);
    if (!dirptr)
    {
        LOGE("fail to open %s", devpath);
        return false;
    }

    while ((entptr = readdir(dirptr)))
    {
        int busnum;
        int devnum;
        if (entptr->d_name[0] == '.')
            continue;

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/busnum", entptr->d_name);
        busnum = file_get_value(devpath, 10);

        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/devnum", entptr->d_name);
        devnum = file_get_value(devpath, 10);

        if (usbdev->busnum == busnum && usbdev->devnum == devnum)
        {
            snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/devpath", entptr->d_name);
            file_get_content(devpath, usbdev->devpath, array_len(usbdev->devpath));
            found = true;
            break;
        }
    }
    closedir(dirptr);

    LOGI("find device %s", usbdev->devpath);

    return found;
}

bool get_vid_pid_from_devpath(QuectelUSBDev *usbdev)
{
    DIR *dirptr = NULL;
    struct dirent *entptr = NULL;
    char devpath[512];
    bool found = false;

    snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/");
    dirptr = opendir(devpath);
    if (!dirptr)
    {
        LOGE("fail to open %s", devpath);
        return false;
    }

    while ((entptr = readdir(dirptr)))
    {
        char port[512];
        if (entptr->d_name[0] == '.')
            continue;

        // the former devpath?
        snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/devpath", entptr->d_name);
        file_get_content(devpath, port, array_len(port));

        if (!strncasecmp(usbdev->devpath, port, strlen(port)))
        {
            snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/busnum", entptr->d_name);
            usbdev->busnum = file_get_value(devpath, 10);

            snprintf(devpath, array_len(devpath), "/sys/bus/usb/devices/%s/devnum", entptr->d_name);
            usbdev->devnum = file_get_value(devpath, 10);
            found = true;
            break;
        }
    }
    closedir(dirptr);

    snprintf(devpath, array_len(devpath), "/dev/bus/usb/%03d/%03d", usbdev->busnum, usbdev->devnum);
    LOGI("find device %s", devpath);

    return found;
}

static bool usbfs_is_kernel_driver_alive(int fd, int ifno)
{
    struct usbfs_driverinfo
    {
        unsigned int interface;
        char driver[255 + 1];
    };

    struct usbfs_driverinfo drvinfo;
    drvinfo.interface = ifno;
    if (ioctl(fd, USBDEVFS_GETDRIVER, &drvinfo) < 0)
    {
        if (errno != ENODATA)
            LOGE("ioctl USBDEVFS_GETDRIVER failed, for %s", strerror(errno));
        return false;
    }
    LOGI("interface %d has beed occupied by the driver %s", ifno, drvinfo.driver);

    return true;
}

int usbfs_detach_kernel_driver(int fd, int ifno)
{
    struct usbfs_ioctl
    {
        int ifno;       /* interface 0..N ; negative numbers reserved */
        int ioctl_code; /* MUST encode size + direction of data so the
			 * macros in <asm/ioctl.h> give correct values */
        void *data;     /* param descptr (in, or out) */
    };
#define IOCTL_USBFS_DISCONNECT _IO('U', 22)
#define IOCTL_USBFS_CONNECT _IO('U', 23)

    struct usbfs_ioctl operate;
    operate.data = NULL;
    operate.ifno = ifno;
    operate.ioctl_code = IOCTL_USBFS_DISCONNECT;

    int ret = ioctl(fd, USBDEVFS_IOCTL, &operate);
    if (ret < 0)
        LOGE("detach kernel driver failed");
    else
        LOGI("detach kernel driver success");
    return ret;
}

/**
 * notice:
 * most of Quectel modem (Qualcomm chipset inside) will cone up with vendor id 0x05c6 product id 0x9008 
 * if stay in upgrade mode using firehose down protocol.
 */
modem_state fu_quectel_modem_state(QuectelUSBDev *usbdev)
{
    fu_quectel_scan_usb_device(usbdev);
    if (usbdev->idVendor == QUECTEL_USB_NORMAL_VID)
        return STATE_NORMAL;

    if (usbdev->idVendor == QUECTEL_USB_EDL_VID && usbdev->idProduct == QUECTEL_USB_EDL_PID)
        return STATE_EDL;

    return STATE_INVALID;
}

bool fu_quectel_open_usb_device(QuectelUSBDev *usbdev)
{
    char devpath[512];
    int fd;

    snprintf(devpath, array_len(devpath), "/dev/bus/usb/%03d/%03d", usbdev->busnum, usbdev->devnum);
    fd = open(devpath, O_RDWR);
    if (fd < 0)
    {
        LOGE("fail open %s for %s", devpath, strerror(errno));
        return false;
    }
    LOGI("open %s with fd %d", devpath, fd);

    if (usbfs_is_kernel_driver_alive(fd, usbdev->ifno))
        usbfs_detach_kernel_driver(fd, usbdev->ifno);

    if (ioctl(fd, USBDEVFS_CLAIMINTERFACE, &(usbdev->ifno)))
    {
        LOGE("fail to claim interface %d, %s", usbdev->ifno, strerror(errno));
        return false;
    }
    usbdev->fd = fd;

    return true;
}

void fu_quectel_close_usb_device(QuectelUSBDev *usbdev)
{
    close(usbdev->fd);
    memset(usbdev, 0, sizeof(QuectelUSBDev));
}

bool fu_quectel_usb_device_send(QuectelUSBDev *usbdev, uint8_t *buffer, int datalen)
{
    struct usbdevfs_urb bulk;
    struct usbdevfs_urb *urb = &bulk;
    int n = -1;

    memset(urb, 0, sizeof(struct usbdevfs_urb));
    urb->type = USBDEVFS_URB_TYPE_BULK;
    urb->endpoint = usbdev->ep_bulk_out;

    urb->status = -1;
    urb->buffer = (void *)buffer;
    urb->buffer_length = (datalen > max_bulk_tx_size) ? max_bulk_tx_size : datalen;
    urb->usercontext = urb;
    urb->flags = 0;

    //         if ((datalen <= max_bulk_size) && need_zlp && (len % udev->wMaxPacketSize[bInterfaceNumber]) == 0)
    //         {
    // #ifndef USBDEVFS_URB_ZERO_PACKET
    // #define USBDEVFS_URB_ZERO_PACKET 0x40
    // #endif
    //             urb->flags = USBDEVFS_URB_ZERO_PACKET;
    //         }
    //         else

    do
    {
        n = ioctl(usbdev->fd, USBDEVFS_SUBMITURB, urb);
    } while ((n < 0) && (errno == EINTR));

    if (n != 0)
    {
        LOGE("ioctl USBDEVFS_SUBMITURB ret=%d, %s", n, strerror(errno));
        return false;
    }

    do
    {
        urb = NULL;
        n = ioctl(usbdev->fd, USBDEVFS_REAPURB, &urb);
    } while ((n < 0) && (errno == EINTR));

    if (n != 0)
    {
        LOGE("ioctl USBDEVFS_REAPURB ret=%d %s", n, strerror(errno));
    }

    //LOGE<<"[ urb @%p status = %d, actual = %d ]\n", urb, urb->status, urb->actual_length);

    if (urb && urb->status == 0 && urb->actual_length)
        return urb->actual_length;

    return false;
}

bool fu_quectel_usb_device_recv(QuectelUSBDev *usbdev, uint8_t *buffer, int datalen)
{
    struct usbdevfs_bulktransfer bulk;
    int n = -1;

    bulk.ep = usbdev->ep_bulk_in;
    bulk.len = (datalen > max_bulk_rx_size) ? max_bulk_rx_size : datalen;
    bulk.data = (void *)buffer;
    bulk.timeout = TIMEOUT;

    n = ioctl(usbdev->fd, USBDEVFS_BULK, &bulk);
    if (n <= 0)
    {
        if (errno == ETIMEDOUT)
        {
            LOGE("usb bulk in timeout");
            n = 0;
        }
        else
            LOGE("usb bulk in error, for %s", strerror(errno));
    }

    return (n == datalen);
}

bool fu_quectel_usb_device_switch_mode(QuectelUSBDev *usbdev)
{
    int max_try = 5;
    uint8_t edl_command[] = {0x4b, 0x65, 0x01, 0x00, 0x54, 0x0f, 0x7e};

    while (fu_quectel_modem_state(usbdev) != STATE_EDL)
    {
        fu_quectel_usb_device_send(usbdev, edl_command, array_len(edl_command));
        usleep(3);
        max_try--;
        if (!max_try)
            break;
    }

    return (fu_quectel_modem_state(usbdev) == STATE_EDL);
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
    uint8_t buffer[FH_TX_MAX];
    int imgfd = open(img, O_RDONLY);
    int remain_len = get_file_size(imgfd);

    do
    {
        int tx_size = (remain_len > FH_TX_MAX) ? FH_TX_MAX : sector_size;
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

int main()
{
    QuectelUSBDev usbdev;
    fu_quectel_scan_usb_device(&usbdev);

    fu_quectel_open_usb_device(&usbdev);
}