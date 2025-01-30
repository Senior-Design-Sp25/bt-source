
#ifndef __I2S_SOURCE_H__
#define __I2S_SOURCE_H__

#define I2S_GPIO_BCLK (25) 	/* 25 I2S Bit Clock GPIO pin */ 
#define I2S_GPIO_WS (26)	/* 26 I2S Word Select (LRCLK) GPIO pin */
#define I2S_GPIO_D_IN (27)	/* 27 I2S Data Out GPIO pin */

/* init i2s and create ring buf */
void init_i2s(void);


#endif /* __I2S_SOURCE_H__ */