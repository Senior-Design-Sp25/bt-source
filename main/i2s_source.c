#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "driver/i2s_std.h"
#include "hal/i2s_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "common.h" 
#include "i2s_source.h"

/* Variable declarations */
i2s_chan_handle_t rx_channel;	/* I2S handle for audio channel (v5.0 API) */
static TaskHandle_t i2s_handle = NULL; /* I2S audio input to DMA task handle */
RingbufHandle_t i2s_buf = NULL; /* Ringbuffer handle for audio stream */
// static bool dropping = false; /*State variable */
static BaseType_t retRing; /* save ringbuffer response for debugging packet loss */

static void i2s_data_handler(void *arg);

#pragma region init

void init_i2s(void)
{
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    channel_config.auto_clear = true; /* Automatically clear DMA TX
                        buffer */
    i2s_std_config_t standard_config = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,
        .bclk = I2S_GPIO_BCLK,
        .ws = I2S_GPIO_WS,
        .dout = I2S_GPIO_UNUSED,
        .din = I2S_GPIO_D_IN,
        .invert_flags = {
    .mclk_inv = false,
    .bclk_inv = false,
    .ws_inv = false,
        },
    },
    };

    ESP_ERROR_CHECK(i2s_new_channel(&channel_config, NULL, &rx_channel));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_channel, &standard_config));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_channel));
    ESP_LOGI(BT_SOURCE_TAG, "I2S Configuration successfull");

    /* Setup ring buffer for audio data */
    i2s_buf = xRingbufferCreate(40960, RINGBUF_TYPE_BYTEBUF);
    if (i2s_buf == NULL) {
    ESP_LOGE(BT_SOURCE_TAG, "Failed to create ring buffer for I2S");
    }

    /* Create I2S task handler */
    BaseType_t task_ret = xTaskCreate(i2s_data_handler, "I2S Task", 2048,
                    NULL, configMAX_PRIORITIES - 3, &i2s_handle);
    if (task_ret == pdPASS) {
    ESP_LOGI(BT_SOURCE_TAG, "Successfully created I2S handler task");
    } else {
    ESP_LOGE(BT_SOURCE_TAG, "Failed to create I2S handler task: %d", task_ret);
    }
}

#pragma endregion init


/* RECEIVE I2S DATA */
static void i2s_data_handler(void *arg)
{
  uint8_t *data = malloc(4096);
    if (!data) {
        ESP_LOGE(BT_SOURCE_TAG, "[echo] No memory for read data buffer");
        abort();
    }
  size_t bytes_read = 0;
  
  for (;;) {
    // read data from rx channel
    i2s_channel_read(rx_channel, data, 4096, &bytes_read, portMAX_DELAY);

    // write data to ring buff
    retRing = xRingbufferSendFromISR(i2s_buf, (void *)data, bytes_read, NULL);

    /* Determine if packets are being dropped or not */
    // if (retRing == pdFALSE && !dropping) {
    //     ESP_LOGW(BT_SOURCE_TAG, "************** DROPPED INCOMING DATA *******************");
    //     dropping = true;
    // } else if (retRing == pdTRUE && dropping) {
    //     ESP_LOGI(BT_SOURCE_TAG, "************** SENDING INCOMING DATA *******************");
    //     dropping = false;
    // }
  }
}