/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-plugin.h"

#define FU_TYPE_FIREHOSE_DEVICE (fu_firehose_device_get_type ())
G_DECLARE_FINAL_TYPE (FuFirehoseDevice, fu_firehose_device, FU, FIREHOSE_DEVICE, FuUsbDevice)

#include <syslog.h>
#define LOGI(fmt, args...) syslog(LOG_USER | LOG_INFO,    "===============quectel %s_%d " fmt, __func__, __LINE__, ##args)