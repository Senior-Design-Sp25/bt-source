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

#define BUFFER_SIZE 960

/* Variable declarations */
i2s_chan_handle_t rx_channel;	/* I2S handle for audio channel (v5.0 API) */
static TaskHandle_t i2s_handle = NULL; /* I2S audio input to DMA task handle */
RingbufHandle_t i2s_buf = NULL; /* Ringbuffer handle for audio stream */
RingbufHandle_t upsamp_buf = NULL; /* Ringbuffer handle for audio stream */

// static bool dropping = false; /*State variable */
static BaseType_t retRing; /* save ringbuffer response for debugging packet loss */

static void i2s_data_handler(void *arg);
static void upsampler(void *arg);

#pragma region init

void init_i2s(void)
{
    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    channel_config.auto_clear = true; /* Automatically clear DMA TX
                        buffer */
    i2s_std_config_t standard_config = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
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
    i2s_buf = xRingbufferCreate(16384, RINGBUF_TYPE_BYTEBUF);
    if (i2s_buf == NULL) {
    ESP_LOGE(BT_SOURCE_TAG, "Failed to create ring buffer for I2S");
    }

    upsamp_buf = xRingbufferCreate(40960, RINGBUF_TYPE_BYTEBUF);
    if (upsamp_buf == NULL) {
    ESP_LOGE(BT_SOURCE_TAG, "Failed to create ring buffer for upsamp");
    }

    /* Create I2S task handler */
    BaseType_t task_ret = xTaskCreatePinnedToCore(i2s_data_handler, "I2S Task", 4096,
                    NULL, configMAX_PRIORITIES - 2, &i2s_handle, 1);
    if (task_ret == pdPASS) {
    ESP_LOGI(BT_SOURCE_TAG, "Successfully created I2S handler task");
    } else {
    ESP_LOGE(BT_SOURCE_TAG, "Failed to create I2S handler task: %d", task_ret);
    }

    task_ret = xTaskCreatePinnedToCore(upsampler, "upsample Task", 16384,
                    NULL, configMAX_PRIORITIES - 2, &i2s_handle, 1);
}

#pragma endregion init

int my_ceil(double x) {
    int i = (int)x;
    return (x > i) ? (i + 1) : i;
}

void upsample_linear_16bit(short *input, int input_length, short *output, int output_length) {
    double ratio = (double)(input_length - 1) / (output_length - 1);
    
    for (int i = 0; i < output_length; i++) {
        double pos = i * ratio;
        int pos_floor = (int)pos;
        int pos_ceil = (pos_floor == input_length - 1) ? pos_floor : pos_floor + 1;
        double frac = pos - pos_floor;
        
        /* Linear interpolation */
        double result = input[pos_floor] * (1.0 - frac) + input[pos_ceil] * frac;
        
        /* Clamp to 16-bit range */
        if (result > 32767.0) result = 32767.0;
        if (result < -32768.0) result = -32768.0;
        
        output[i] = (short)result;
    }
}

void convert_stereo(short *input, int input_size, short *output, int output_size) {
    for (int i=0; i<input_size; i++) {
        output[i*2] = input[i];
        output[(i*2)+1] = input[i];
    }
}

static void upsampler(void *arg) {
  uint8_t buffer[BUFFER_SIZE];
  size_t current_buffer_size = 0; 
  uint8_t *data = NULL;
  size_t size = 0;
  int input_samples_count = BUFFER_SIZE / sizeof(short);
  int output_samples_count = my_ceil(input_samples_count * (44100.0 / 16000.0));
  size_t upsampled_bytes = output_samples_count * sizeof(short);
  uint8_t out[upsampled_bytes];
  short *out_16 = (short *)out;
  short *buff_16 = (short *)buffer;
  uint8_t out_stero[upsampled_bytes*2];
  int output_samples_count_stereo = output_samples_count*2;
  short *out_stero_16 = (short *)out_stero;


  for (;;) {
    size_t bytes_to_receive = BUFFER_SIZE - current_buffer_size;

    data = (uint8_t *)xRingbufferReceiveUpTo(i2s_buf, &size, (TickType_t)portMAX_DELAY, bytes_to_receive);
    
    if (size > 0) {
        // Copy received data to buffer
        memcpy(buffer + current_buffer_size, data, size);
        current_buffer_size += size;
        
        // Return the ring buffer item
        vRingbufferReturnItem(i2s_buf, (void *)data);
        
        // If buffer is full, send and reset
        if (current_buffer_size == BUFFER_SIZE) {

            upsample_linear_16bit(buff_16, input_samples_count, out_16, output_samples_count);

            convert_stereo(out_16, output_samples_count, out_stero_16, output_samples_count_stereo);

            xRingbufferSend(upsamp_buf, out_stero, upsampled_bytes*2, (TickType_t)portMAX_DELAY);
            current_buffer_size = 0;
        }
    }
  }
}

/* RECEIVE I2S DATA */
 static void i2s_data_handler(void *arg)
 {
   uint8_t *data = malloc(4096);
   size_t bytes_read = 0;
  
   for (;;) {
     // read data from rx channel
     i2s_channel_read(rx_channel, data, 4096, &bytes_read, portMAX_DELAY);

     // write data to ring buff
     retRing = xRingbufferSendFromISR(i2s_buf, (void *)data, bytes_read, NULL);
   }
 }


    /* Determine if packets are being dropped or not */
    // if (retRing == pdFALSE && !dropping) {
    //     ESP_LOGW(BT_SOURCE_TAG, "************** DROPPED INCOMING DATA *******************");
    //     dropping = true;
    // } else if (retRing == pdTRUE && dropping) {
    //     ESP_LOGI(BT_SOURCE_TAG, "************** SENDING INCOMING DATA *******************");
    //     dropping = false;
    // }