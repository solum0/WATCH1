#ifndef APP_VERSION_H
#define APP_VERSION_H

#include <stdint.h>

/* Increment this value before building an OTA image. */
#define APP_FW_VERSION_MAJOR       0U
#define APP_FW_VERSION_MINOR       3U
#define APP_FW_VERSION_PATCH       9U

#define APP_FW_VERSION             ((uint32_t)((APP_FW_VERSION_MAJOR << 16U) | \
                                               (APP_FW_VERSION_MINOR << 8U) | \
                                               APP_FW_VERSION_PATCH))

#endif
