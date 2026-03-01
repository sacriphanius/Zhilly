#include "t_embed_board.h"
#include "application.h"
#include "audio/audio_codec.h"
#include "audio/codecs/no_audio_codec.h"
#include "backlight.h"
#include "display/display.h"
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
#include "display/emote_display.h"
#else
#include "display/lcd_display.h"
#endif
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include "driver/spi_common.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "TEmbedBoard"

typedef struct {
    uint8_t cmd;
    uint8_t data[14];
    uint8_t len;
} lcd_cmd_t;

static const lcd_cmd_t lcd_st7789v[] = {
    {0x11, {0}, 0 | 0x80},
    {0x3A, {0X05}, 1},
    {0xB2, {0X0B, 0X0B, 0X00, 0X33, 0X33}, 5},
    {0xB7, {0X75}, 1},
    {0xBB, {0X28}, 1},
    {0xC0, {0X2C}, 1},
    {0xC2, {0X01}, 1},
    {0xC3, {0X1F}, 1},
    {0xC6, {0X13}, 1},
    {0xD0, {0XA7}, 1},
    {0xD0, {0XA4, 0XA1}, 2},
    {0xD6, {0XA1}, 1},
    {0xE0,
     {0XF0, 0X05, 0X0A, 0X06, 0X06, 0X03, 0X2B, 0X32, 0X43, 0X36, 0X11, 0X10, 0X2B, 0X32},
     14},
    {0xE1,
     {0XF0, 0X08, 0X0C, 0X0B, 0X09, 0X24, 0X2B, 0X22, 0X43, 0X38, 0X15, 0X16, 0X2F, 0X37},
     14},
};

TEmbedBoard::TEmbedBoard() : boot_button_(BOOT_BUTTON_GPIO), pwr_button_(PWR_BUTTON_GPIO) {
    gpio_set_direction(BOARD_PWR_EN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_PWR_EN_PIN, 1);

    backlight_ = new PwmBacklight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
    backlight_->SetBrightness(75);

    gpio_set_direction(DISPLAY_SPI_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DISPLAY_SPI_CS_PIN, 1);
    gpio_set_direction(SD_CARD_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_CARD_CS_PIN, 1);
    gpio_set_direction(LORA_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LORA_CS_PIN, 1);

    InitSpi();
    InitLcdDisplay();
    InitializeButtons();
    InitializeSdCard();
}

void TEmbedBoard::InitSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
    buscfg.miso_io_num = DISPLAY_SPI_MISO_PIN;
    buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

void TEmbedBoard::InitLcdDisplay() {
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
    io_config.dc_gpio_num = DISPLAY_DC_PIN;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;  // GPIO_NUM_NC — IO40 shared with I2S_WCLK
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

    esp_lcd_panel_reset(panel);
    esp_lcd_panel_init(panel);

    for (uint8_t i = 0; i < (sizeof(lcd_st7789v) / sizeof(lcd_cmd_t)); i++) {
        esp_lcd_panel_io_tx_param(panel_io, lcd_st7789v[i].cmd, lcd_st7789v[i].data,
                                  lcd_st7789v[i].len & 0x7f);
        if (lcd_st7789v[i].len & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(120));
        }
    }

    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_set_gap(panel, 0, 35);
    esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
    esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    esp_lcd_panel_disp_on_off(panel, true);

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
    display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#else
    display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, 0,
                                 DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#endif
}

void TEmbedBoard::InitializeButtons() {
    boot_button_.OnClick([this]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() == kDeviceStateStarting) {
            EnterWifiConfigMode();
            return;
        }
        app.ToggleChatState();
    });

    // Deep Sleep on long press of power button (GPIO 6)
    pwr_button_.OnLongPress([this]() {
        ESP_LOGI(TAG, "Entering Deep Sleep Mode...");
        auto& app = Application::GetInstance();
        app.SetDeviceState(kDeviceStateIdle);

        if (display_) {
            display_->SetChatMessage("system", "Uyku Moduna Geciliyor...");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (backlight_) {
            backlight_->SetBrightness(0);
        }

        app.ResetProtocol();

        ESP_LOGI(TAG, "Waiting for power button to be released...");
        while (gpio_get_level(PWR_BUTTON_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        ESP_LOGI(TAG, "Powering down peripherals");
        gpio_set_level(BOARD_PWR_EN_PIN, 0);

        ESP_LOGI(TAG, "Configuring EXT0 wakeup and entering deep sleep");
        rtc_gpio_init(PWR_BUTTON_GPIO);
        rtc_gpio_set_direction(PWR_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(PWR_BUTTON_GPIO);
        rtc_gpio_pulldown_dis(PWR_BUTTON_GPIO);
        esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0);

        vTaskDelay(pdMS_TO_TICKS(500));
        esp_deep_sleep_start();
    });
}

void TEmbedBoard::InitializeSdCard() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 0,
        .disk_status_check_enable = true,
    };

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SD_CARD_MOSI_PIN;
    buscfg.miso_io_num = SD_CARD_MISO_PIN;
    buscfg.sclk_io_num = SD_CARD_SCK_PIN;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = 4000;

    spi_bus_initialize(SDCARD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SDCARD_SPI_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CARD_CS_PIN;
    slot_config.host_id = SDCARD_SPI_HOST;

    sdmmc_card_t* card;
    esp_err_t ret =
        esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD card mounted at %s", SDCARD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
}

AudioCodec* TEmbedBoard::GetAudioCodec() {
    static NoAudioCodecSimplexPdm audio_codec(
        AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_SPK_BCLK, AUDIO_I2S_SPK_WS,
        AUDIO_I2S_SPK_DOUT, AUDIO_I2S_MIC_CLK, AUDIO_I2S_MIC_DATA);
    return &audio_codec;
}

Display* TEmbedBoard::GetDisplay() { return display_; }

Backlight* TEmbedBoard::GetBacklight() { return backlight_; }

DECLARE_BOARD(TEmbedBoard);
