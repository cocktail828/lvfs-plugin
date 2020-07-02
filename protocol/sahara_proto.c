
#include <endian.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "sahara_proto.h"
#include "../fu-quectel-common.h"
#include "../fu-quectel-device.h"

#define SAHARA_MAX_PKT (8 * 1024)

static gchar flash_tool_image[128];
static uint8_t buffer[SAHARA_MAX_PKT];
static int datalen;
static int sahara_version = 0x02;
static int sahara_version_compatible = 0;
static int imgfd;
static QuectelUSBDev *usbdev;
static int timeout;

static sahara_status sahara_status_codes[] = {
    {0x00, "1.0", "Success"},
    {0x01, "1.0", "Invalid command received in current state"},
    {0x02, "1.0", "Protocol mismatch between host and target"},
    {0x03, "1.0", "Invalid target protocol version"},
    {0x04, "1.0", "Invalid host protocol version"},
    {0x05, "1.0", "Invalid packet size received"},
    {0x06, "1.0", "Unexpected image ID received 1"},
    {0x07, "1.0", "Invalid image header size received"},
    {0x08, "1.0", "Invalid image data size received"},
    {0x09, "1.0", "Invalid image type received"},
    {0x0A, "1.0", "Invalid transmission length"},
    {0x0B, "1.0", "Invalid reception length"},
    {0x0C, "1.0", "General transmission or reception error"},
    {0x0D, "1.0", "Error while transmitting READ_DATA packet"},
    {0x0E, "1.0", "Cannot receive specified number of program headers"},
    {0x0F, "1.0", "Invalid data length received for program headers"},
    {0x10, "1.0", "Multiple shared segments found in ELF image"},
    {0x11, "1.0", "Uninitialized program header location"},
    {0x12, "1.0", "Invalid destination address"},
    {0x13, "1.0", "Invalid data size received in image header"},
    {0x14, "1.0", "Invalid ELF header received"},
    {0x15, "1.0", "Unknown host error received in HELLO_RESP"},
    {0x16, "1.0", "Timeout while receiving data"},
    {0x17, "1.0", "Timeout while transmitting data"},
    {0x18, "2.0", "Invalid mode received from host"},
    {0x19, "2.0", "Invalid memory read access"},
    {0x1A, "2.0", "Host cannot handle read data size requested"},
    {0x1B, "2.0", "Memory debug not supported"},
    {0x1C, "2.1", "Invalid mode switch"},
    {0x1D, "2.1", "Failed to execute command"},
    {0x1E, "2.1", "Invalid parameter passed to command execution"},
    {0x1F, "2.1", "Unsupported client command received"},
    {0x20, "2.1", "Invalid client command received for data response"},
    {0x21, "2.4", "Failed to authenticate hash table"},
    {0x22, "2.4", "Failed to verify hash for a given segment of ELF image"},
    {0x23, "2.4", "Failed to find hash table in ELF image"},
    {0x24, "2.4", "Target failed to initialize"},
    {0x25, "2.5", "Failed to authenticate generic image"},
};

// default mode, SAHARA_MODE_IMAGE_TX_COMPLETE
static sahara_hello_resp *new_sahara_hello_resp(sahara_mode mode)
{
    sahara_hello_resp *pkt = (sahara_hello_resp *)buffer;

    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = sizeof(sahara_hello_resp);

    pkt->command = htole32(SAHARA_HELLO_RESP);
    pkt->length = htole32(datalen);
    pkt->version = htole32(sahara_version);
    pkt->version_compatible = htole32(sahara_version_compatible);
    pkt->status = 0;
    pkt->mode = htole32(mode);

    return (sahara_hello_resp *)buffer;
}

static uint8_t *new_sahara_raw_data(int _offset, int _datalen)
{
    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = _datalen;

    if (!imgfd)
        imgfd = open(flash_tool_image, O_RDONLY);

    lseek(imgfd, _offset, SEEK_SET);
    if (read(imgfd, buffer, _datalen) < 0)
    {
        datalen = 0;
        return NULL;
    }

    return (uint8_t *)buffer;
}

static sahara_done *new_sahara_done(void)
{
    sahara_done *pkt = (sahara_done *)buffer;

    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = sizeof(sahara_done);

    pkt->command = htole32(SAHARA_DONE);
    pkt->length = htole32(datalen);

    return (sahara_done *)buffer;
}

static sahara_reset *new_sahara_reset(void)
{
    sahara_reset *pkt = (sahara_reset *)buffer;

    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = sizeof(sahara_reset);

    pkt->command = htole32(SAHARA_RESET);
    pkt->length = htole32(datalen);

    return (sahara_reset *)buffer;
}

static sahara_switch_mode *new_sahara_switch_mode(sahara_mode mode)
{
    sahara_switch_mode *pkt = (sahara_switch_mode *)buffer;

    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = sizeof(sahara_switch_mode);

    pkt->command = htole32(SAHARA_CMD_SWITCH_MODE);
    pkt->length = htole32(datalen);
    pkt->mode = htole32(mode);

    return (sahara_switch_mode *)buffer;
}

static sahara_reset_machine *new_sahara_reset_machine(void)
{
    sahara_reset_machine *pkt = (sahara_reset_machine *)buffer;

    memset(buffer, 0, SAHARA_MAX_PKT);
    datalen = sizeof(sahara_reset_machine);

    pkt->command = htole32(SAHARA_RESET_MACHINE);
    pkt->length = htole32(datalen);

    return (sahara_reset_machine *)buffer;
}

static sahara_command sahara_command_type(uint8_t *data)
{
    sahara_common_header *header = (sahara_common_header *)data;
    if (!data)
        return SAHARA_INVALID;
    return le32toh(header->command);
}

static const sahara_status *sahara_get_status_code(uint32_t status)
{
    static sahara_status current_status = {
        .status = 0,
        .version = "--",
        .description = "sahara unknow error",
    };

    for (guint i = 0; i < array_len(sahara_status_codes); i++)
    {
        if (sahara_status_codes[i].status == status)
            return &sahara_status_codes[i];
    }

    current_status.status = status;
    return &current_status;
}

static int sahara_tx_finish(sahara_end_img_transfer *pkt)
{
    const sahara_status *status_code = sahara_get_status_code(le32toh(pkt->status));

    close(imgfd);
    imgfd = 0;
    return status_code->status != 0;
}

static void sahara_dump_message(uint8_t *msg, int len)
{
    char _buffer[1024] = {'\0'};
    for (int i = 0; i < len; i++)
        snprintf(_buffer, strlen(_buffer), "%02x ", (int)msg[i]);
    LOGI("%s", _buffer);
}

static void sahara_machine_init(QuectelUSBDev *_dev)
{
    usbdev = _dev;
    datalen = 0;
    datalen = 0;
    timeout = 5000;
}

static void sahara_machine_cleanup(void)
{
    if (imgfd)
    {
        close(imgfd);
        imgfd = 0;
    }
}

static int sahara_recv_cmd(void)
{
    memset(buffer, 0, SAHARA_MAX_PKT);
    return fu_quectel_usb_device_recv(usbdev, buffer, array_len(buffer), timeout);
}

static int sahara_send_cmd(void)
{
    return fu_quectel_usb_device_send(usbdev, buffer, datalen, timeout);
}

int do_image_transfer(void *_usbdev, const char *_image)
{
    sahara_common_header *header = (sahara_common_header *)buffer;
    sahara_machine_init((QuectelUSBDev*)_usbdev);
    do
    {
        if (sahara_recv_cmd())
        {
            LOGE("sahara recv command fail");
            return -1;
        }

        switch (sahara_command_type(buffer))
        {
        case SAHARA_HELLO:
        {
            LOGI("<- sahara_hello");
            new_sahara_hello_resp(SAHARA_MODE_IMAGE_TX_COMPLETE);
            if (sahara_send_cmd())
            {
                LOGI("sahara_send_cmd error");
                return -1;
            }
            LOGI("-> sahara_hello_resp");
            break;
        }
        case SAHARA_READ_DATA:
        {
            sahara_read_data *pkt = (sahara_read_data *)header;
            LOGI("<- sahara_read_data offset %u len %u", le32toh(pkt->offset), le32toh(pkt->datalen));

            snprintf(flash_tool_image, sizeof(flash_tool_image), "%s", _image);
            new_sahara_raw_data(le32toh(pkt->offset), le32toh(pkt->datalen));
            if (sahara_send_cmd())
            {
                LOGI("sahara_send_cmd error");
                return -1;
            }
            LOGI("-> sahara_raw_data");
            break;
        }
        case SAHARA_END_IMG_TRANSFER:
        {
            LOGI("<- sahara_end_image_transfer");
            if (sahara_tx_finish((sahara_end_img_transfer *)header))
            {
                LOGI("target send SAHARA_END_IMG_TRANSFER with error");
                return -1;
            }

            new_sahara_done();
            if (sahara_send_cmd())
            {
                LOGI("sahara_send_cmd error");
                return -1;
            }
            LOGI("-> sahara_done");
            break;
        }
        case SAHARA_DONE_RESP:
        {
            LOGI("<- sahara_done_resp");
            LOGI("image has been successfully transfered");
            return 0;
        }
        default:
            LOGI("invalid sahara command recevied");
        }
    } while (1);
    return 0;
}
