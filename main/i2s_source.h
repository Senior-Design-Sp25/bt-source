
#ifndef __I2S_SOURCE_H__
#define __I2S_SOURCE_H__

#define I2S_GPIO_BCLK GPIO_NUM_25 	/* 25 I2S Bit Clock GPIO pin */ 
#define I2S_GPIO_WS GPIO_NUM_26	/* 26 I2S Word Select (LRCLK) GPIO pin */
#define I2S_GPIO_D_IN GPIO_NUM_27	/* 27 I2S Data Out GPIO pin */

/* init i2s and create ring buf */
void init_i2s(void);

void init_SD(void);

#endif /* __I2S_SOURCE_H__ */