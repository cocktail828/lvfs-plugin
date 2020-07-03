#pragma once

#include <stdint.h>
#include <errno.h>

typedef uint8_t bool;
#define true 1
#define false 0

typedef enum
{
	CMD_ERASE,
	CMD_PROGRAM,
	CMD_CONFIG,
} command_type;

typedef struct
{
	command_type type;
	char *hash;
	char *data;
	char *img;
	int sector_size;
} QuectelFirmware;

/**
 * APIs of firmware
 */
QuectelFirmware *fu_quectel_firmware_parser(const char *xml);
bool fu_quectel_firmware_varify(QuectelFirmware *fm);
void fu_quectel_firmware_cleanup(QuectelFirmware *fm);
