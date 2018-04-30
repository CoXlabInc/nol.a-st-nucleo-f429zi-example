#ifndef USBD_CONF_H
#define USBD_CONF_H

#include <typedef.h>

#define USBD_MAX_NUM_INTERFACES               1
#define USBD_MAX_NUM_CONFIGURATION            1
#define USBD_MAX_STR_DESC_SIZ                 0x100
#define USBD_SUPPORT_USER_STRING              0
#define USBD_SELF_POWERED                     1
#define USBD_DEBUG_LEVEL                      0

void *dynamicMalloc(uint16_t len);
void dynamicFree(void *ptr);

#define USBD_malloc     dynamicMalloc
#define USBD_free       dynamicFree
#define USBD_memset     memset
#define USBD_memcpy     memcpy

#define USBD_UsrLog(...)
#define USBD_ErrLog(...)
#define USBD_DbgLog(...)

#endif /* USBD_CONF_H */
