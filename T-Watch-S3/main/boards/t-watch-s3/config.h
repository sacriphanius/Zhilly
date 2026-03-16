#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// LilyGO T-Watch S3 Board configuration
// MCU: ESP32-S3
// Display: ST7789V 1.54" 240x240
// Audio: MAX98357A (DAC/Amp)
// PMU: AXP2101

// Audio settings
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Audio pins (MAX98357A - No I2C config needed)
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_15
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_46
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_NC // Output only (DAC)

// PDM Microphone pins (SPM1423)
#define AUDIO_I2S_MIC_GPIO_CLK  GPIO_NUM_44
#define AUDIO_I2S_MIC_GPIO_DATA GPIO_NUM_47

// IR TX pin (from Bruce firmware for T-Watch S3)
#define IR_TX_PIN            GPIO_NUM_2

// I2C pins (shared for AXP2101, BMA423, PCF8563)
#define I2C_SDA_PIN          GPIO_NUM_10
#define I2C_SCL_PIN          GPIO_NUM_11

// I2C pins for Touchscreen (Separate bus)
#define I2C_TOUCH_SDA_PIN    GPIO_NUM_39
#define I2C_TOUCH_SCL_PIN    GPIO_NUM_40

#define AXP2101_ADDR         0x34

// Buttons
#define BOOT_BUTTON_GPIO     GPIO_NUM_0
#define TOUCH_INT_PIN        GPIO_NUM_16

// Display ST7789 (SPI)
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_13
#define DISPLAY_SPI_SCLK_PIN GPIO_NUM_18
#define DISPLAY_SPI_CS_PIN   GPIO_NUM_12
#define DISPLAY_DC_PIN       GPIO_NUM_38
#define DISPLAY_RST_PIN      GPIO_NUM_NC // Not connected
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_45
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#endif // _BOARD_CONFIG_H_
