#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE         24000
#define AUDIO_OUTPUT_SAMPLE_RATE        24000

#define AUDIO_INPUT_REFERENCE           true

#define AUDIO_I2S_GPIO_MCLK             GPIO_NUM_13
#define AUDIO_I2S_GPIO_WS               GPIO_NUM_10
#define AUDIO_I2S_GPIO_BCLK             GPIO_NUM_12
#define AUDIO_I2S_GPIO_DIN              GPIO_NUM_11
#define AUDIO_I2S_GPIO_DOUT             GPIO_NUM_9

#define AUDIO_CODEC_PA_PIN              GPIO_NUM_53
#define AUDIO_CODEC_I2C_SDA_PIN         GPIO_NUM_7
#define AUDIO_CODEC_I2C_SCL_PIN         GPIO_NUM_8
#define AUDIO_CODEC_ES8311_ADDR         ES8311_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO                GPIO_NUM_35

#define DISPLAY_WIDTH 1024
#define DISPLAY_HEIGHT 600

#define LCD_BIT_PER_PIXEL               (16)
#define PIN_NUM_LCD_RST                 GPIO_NUM_23

#define DELAY_TIME_MS                   (3000)
#define LCD_MIPI_DSI_LANE_NUM           (2)    

#define MIPI_DSI_PHY_PWR_LDO_CHAN       (3)
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#define DISPLAY_SWAP_XY                 false
#define DISPLAY_MIRROR_X                false
#define DISPLAY_MIRROR_Y                false

#define DISPLAY_OFFSET_X                0
#define DISPLAY_OFFSET_Y                0

#define DISPLAY_BACKLIGHT_PIN           GPIO_NUM_20
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

#ifndef SDCARD_SDMMC_ENABLED
#define SDCARD_SDMMC_ENABLED            0
#endif

#ifndef SDCARD_SDMMC_BUS_WIDTH

#define SDCARD_SDMMC_BUS_WIDTH          4
#endif

#ifndef SDCARD_SDMMC_CLK_PIN
#define SDCARD_SDMMC_CLK_PIN            GPIO_NUM_43  
#endif
#ifndef SDCARD_SDMMC_CMD_PIN
#define SDCARD_SDMMC_CMD_PIN            GPIO_NUM_44  
#endif
#ifndef SDCARD_SDMMC_D0_PIN
#define SDCARD_SDMMC_D0_PIN             GPIO_NUM_39  
#endif
#ifndef SDCARD_SDMMC_D1_PIN
#define SDCARD_SDMMC_D1_PIN             GPIO_NUM_40  
#endif
#ifndef SDCARD_SDMMC_D2_PIN
#define SDCARD_SDMMC_D2_PIN             GPIO_NUM_41  
#endif
#ifndef SDCARD_SDMMC_D3_PIN
#define SDCARD_SDMMC_D3_PIN             GPIO_NUM_42  
#endif

#ifndef SDCARD_SDSPI_ENABLED
#define SDCARD_SDSPI_ENABLED            1
#endif
#ifndef SDCARD_SPI_HOST
#define SDCARD_SPI_HOST                 SPI3_HOST
#endif
#ifndef SDCARD_SPI_MOSI
#define SDCARD_SPI_MOSI                 GPIO_NUM_44  
#endif
#ifndef SDCARD_SPI_MISO
#define SDCARD_SPI_MISO                 GPIO_NUM_39  
#endif
#ifndef SDCARD_SPI_SCLK
#define SDCARD_SPI_SCLK                 GPIO_NUM_43  
#endif
#ifndef SDCARD_SPI_CS
#define SDCARD_SPI_CS                   GPIO_NUM_42  
#endif

#ifndef SDCARD_MOUNT_POINT
#define SDCARD_MOUNT_POINT              "/sdcard"
#endif

#endif 
