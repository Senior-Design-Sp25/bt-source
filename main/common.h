#ifndef __COMMON_H__
#define __COMMON_H__

#include "freertos/ringbuf.h"

/* DEFINITIONS */
#define BT_SOURCE_TAG "BT_SOURCE"

/* Ringbuffer handle for audio stream */
extern RingbufHandle_t i2s_buf;
extern RingbufHandle_t upsamp_buf;


#endif /* __COMMON_H__ */