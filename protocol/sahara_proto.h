#pragma once

#include <stdint.h>
#include <string.h>

typedef enum
{
    SAHARA_INVALID = 0x00,
    SAHARA_HELLO = 0x01,            // Hello          Target 1.0 Initialize connection and protocol
    SAHARA_HELLO_RESP = 0x02,       // Hello response Host   1.0 Acknowledge connection and protocol sent by target;
                                    //                           also used to set mode of operation for target to execute in
    SAHARA_READ_DATA = 0x03,        // Read data      Target 1.0 Read specified number of bytes from host for a given image
    SAHARA_END_IMG_TRANSFER = 0x04, // End of image transfer
                                    //                Target 1.0 Indicate to host that a single image transfer is complete;
                                    //                           also used to indicate a target failure during an image transfer
    SAHARA_DONE = 0x05,             // Done           Host   1.0 Acknowledgement from host that a single image transfer is complete
    SAHARA_DONE_RESP = 0x06,        // Done response  Target 1.0 Indicate to host:
                                    //                               ■ Target is exiting protocol
                                    //                               ■ Whether or not target expects to re-enter protocol
                                    //                                  to transfer another image
    SAHARA_RESET = 0x07,            // Reset          Host   1.0 Instruct target to perform a reset
    SAHARA_RESET_RESP = 0x08,       // Reset response Target 1.0 Indicate to host that target is about to reset
    SAHARA_MEM_DBG = 0x09,          // Memory debug   Target 2.0 Indicate to host that target has entered a debug mode where it is ready to transfer its system memory contents
    SAHARA_MEM_RD = 0x0A,           // Memory read    Host   2.0 Read specified number of bytes from target’s system memory, starting from a specified address
    SAHARA_CMD_RDY = 0x0B,          // Command ready  Target 2.1 Indicate to host that target is ready to receive client commands
    SAHARA_CMD_SWITCH_MODE = 0x0C,  // Command switch mode
                                    //                Host   2.1 Indicate to target to switch modes:
                                    //                               ■ Image Transfer Pending mode
                                    //                               ■ Image Transfer Complete mode
                                    //                               ■ Memory Debug mode
                                    //                               ■ Command mode
    SAHARA_CMD_EXECUTE = 0x0D,      // Command execute
                                    //                Host   2.1 Indicate to target to execute a given client command
    SAHARA_CMD_EXECUTE_RESP = 0x0E, // Command execute response
                                    //                Target 2.1 Indicate to host that target has executed client command;
                                    //                           also used to indicate status of executed command
    SAHARA_CMD_EXECUTE_DATA = 0x0F, // Command execute data
                                    //                Host   2.1 Indicate to target that host is ready to receive data
                                    //                           resulting from executing previous client command
    SAHARA_64_MEM_DBG = 0x10,       // 64-bit memory debug
                                    //                Target 2.5 Indicate to host that target has entered a debug mode where it is rea
    SAHARA_64_MEM_RD = 0x11,        // 64-bit memory read
                                    //                Host   2.5 Read specified number of bytes from target’s system memory, starting from a specified 64-bit address
    SAHARA_64_RD_DATA = 0x12,       // 64-bit read data
                                    //                Target 2.8 Read specified number of bytes from host for a given 64-bit image
    SAHARA_RESET_MACHINE = 0x13,    // Reset sahara   Host   2.9 Reset Sahara state machine, enter into Sahara entry without target reset state machine
} sahara_command;

typedef enum
{
    SAHARA_MODE_IMAGE_TX_PENDING = 0x0,  // Image transfer Pending mode. Transfer image from the host; after completion, the host
                                         // should expect another image transfer request.
    SAHARA_MODE_IMAGE_TX_COMPLETE = 0x1, // Image transfer Complete mode.Transfer image from the host;
                                         // after completion, the host should not expect another image transfer request
    SAHARA_MODE_MEMORY_DEBUG = 0x2,      // Memory debug mode.The host should
                                         // prepare to receive a memory dump from the target.
    SAHARA_MODE_COMMAND = 0x3,           // Command mode.The host executes operations on the target by sending the appropriate client command to the Sahara
                                         // client running on the target.The client command is interpreted by the Sahara client
                                         // and the corresponding response sent upon execution of the given command.
} sahara_mode;

typedef struct
{
    uint32_t status;
    const char *version;
    const char *description;
} sahara_status;


#define SAHARA_COMMON_HDR \
    uint32_t command;     \
    uint32_t length;

typedef struct
{
    SAHARA_COMMON_HDR
} sahara_common_header;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t version;
    uint32_t version_compatible;
    uint32_t cmd_pkt_len;
    uint32_t mode;
    uint32_t reserve[6];
} sahara_hello;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t version;
    uint32_t version_compatible;
    uint32_t status;
    uint32_t mode;
    uint32_t reserve[6];
} sahara_hello_resp;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t image_id;
    uint32_t offset;
    uint32_t datalen;
} sahara_read_data;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t image_id;
    uint32_t status;
} sahara_end_img_transfer;

typedef struct
{
    SAHARA_COMMON_HDR;
} sahara_done;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t image_transfer_status;
} sahara_done_resp;

typedef struct
{
    SAHARA_COMMON_HDR;
} sahara_reset;

typedef struct
{
    SAHARA_COMMON_HDR;
} sahara_reset_resp;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t mem_tb_addr;
    uint32_t mem_tb_len;
} sahara_debug;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t mode;
} sahara_switch_mode;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t client_cmd;
} sahara_execute;

typedef struct
{
    SAHARA_COMMON_HDR;
    uint32_t client_cmd;
    uint32_t resp_len;
} sahara_execute_resp;

typedef struct
{
    SAHARA_COMMON_HDR;
} sahara_reset_machine;

int do_image_transfer(void *_usbdev, const char *_image);