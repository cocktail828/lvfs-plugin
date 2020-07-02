#include "fu-quectel-firmware.h"

QuectelFirmware *fu_quectel_firmware_parser(const gchar *xml)
{
}

void fu_quectel_firmware_cleanup(QuectelFirmware *fm)
{
}

gboolean fu_quectel_firmware_varify(QuectelFirmware *fm)
{
	gchar *cmd = NULL;
	gboolean status;

	for (guint i = 0; fm[i]; i++)
	{
		if (g_strcmp0(hash(ptr->img), ptr->hash))
		{
			status = FALSE;
			break;
		}
	}

	return status;
}