#pragma once

#include <stdio.h>
#define LOG_PREFIX ""
#define LOGD(fmt, args...) printf(LOG_PREFIX "%s_%d " fmt "\n", __FILE__, __LINE__, ##args)
#define LOGI(fmt, args...) printf(LOG_PREFIX "%s_%d " fmt "\n", __FILE__, __LINE__, ##args)
#define LOGW(fmt, args...) printf(LOG_PREFIX "%s_%d " fmt "\n", __FILE__, __LINE__, ##args)
#define LOGE(fmt, args...) printf(LOG_PREFIX "%s_%d " fmt "\n", __FILE__, __LINE__, ##args)

#define LINE LOGI("%s %d", __func__, __LINE__);

#define array_len(_a) (sizeof(_a) / sizeof(_a[0]))
