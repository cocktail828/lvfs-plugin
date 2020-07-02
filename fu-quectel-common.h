#ifndef _FU_QUECTEL_COMMON_
#define _FU_QUECTEL_COMMON_

#define LOG_PREFIX "plugin-quectel "
#define LOGD(fmt, args...) syslog(LOG_USER|LOG_DEBUG, LOG_PREFIX fmt, ##args);
#define LOGI(fmt, args...) syslog(LOG_USER|LOG_INFO, LOG_PREFIX fmt, ##args);
#define LOGW(fmt, args...) syslog(LOG_USER|LOG_WARNING, LOG_PREFIX fmt, ##args);
#define LOGE(fmt, args...) syslog(LOG_USER|LOG_ERR, LOG_PREFIX fmt, ##args);

// appstream-util generate-guid "USB\VID_2C7C"
#define QUECTEL_COMMON_USB_GUID "22ae45db-f68e-5c55-9c02-4557dca238ec"
#endif //_FU_QUECTEL_COMMON_
