#pragma once
#include <syslog.h>

#define LOG_PREFIX "plugin-quectel "
#define LOGD(fmt, args...) syslog(LOG_USER | LOG_DEBUG, LOG_PREFIX fmt, ##args);
#define LOGI(fmt, args...) syslog(LOG_USER | LOG_INFO, LOG_PREFIX fmt, ##args);
#define LOGW(fmt, args...) syslog(LOG_USER | LOG_WARNING, LOG_PREFIX fmt, ##args);
#define LOGE(fmt, args...) syslog(LOG_USER | LOG_ERR, LOG_PREFIX fmt, ##args);

#define LINE LOGI("%s %d", __func__, __LINE__);

#define array_len(_a) (sizeof(_a) / sizeof(_a[0]))
