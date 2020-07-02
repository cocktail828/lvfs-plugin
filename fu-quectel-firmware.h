#pragma once

#include "fu-plugin.h"

typedef enum
{
	CMD_ERASE,
	CMD_PROGRAM,
	CMD_CONFIG,
} command_type;

typedef struct
{
	command_type type;
	gchar *hash;
	gchar *data;
	gchar *img;
	guint sector_size;
} QuectelFirmware;

/**
 * APIs of firmware
 */
QuectelFirmware *fu_quectel_firmware_parser(const gchar *xml);
gboolean fu_quectel_firmware_varify(QuectelFirmware *fm);
void fu_quectel_firmware_cleanup(QuectelFirmware *fm);
