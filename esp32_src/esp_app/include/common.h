#ifndef COMMON_H
#define COMMON_H

#include "esp_log.h"

#define ERR_PRINT(ret) ESP_LOGE(tag, "%s %s: %d, retcode = %x", __FILE__, __FUNCTION__, __LINE__, ret)
