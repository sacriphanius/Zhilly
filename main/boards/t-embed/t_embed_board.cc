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
#include "mcp_server.h"
#include "settings.h"

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

     
    InitI2c();
    InitSpi();
    InitLcdDisplay();
    InitializeButtons();
    InitializeSdCard();
    InitLedStrip();
}

void TEmbedBoard::InitSpi() {
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
    buscfg.miso_io_num = SD_CARD_MISO_PIN;   
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
    panel_config.reset_gpio_num = DISPLAY_RST_PIN;
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

static int LevelToBrightness(int level) {
    if (level < 0)
        level = 0;
    if (level > 8)
        level = 8;
    return (1 << level) - 1;
}

static StripColor RGBToColor(int red, int green, int blue) {
    if (red < 0)
        red = 0;
    if (red > 255)
        red = 255;
    if (green < 0)
        green = 0;
    if (green > 255)
        green = 255;
    if (blue < 0)
        blue = 0;
    if (blue > 255)
        blue = 255;
    return {static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

void TEmbedBoard::InitLedStrip() {
    led_strip_ = new CircularStrip(WS2812_LED_PIN, WS2812_LED_COUNT);

    Settings settings("led_strip");
    int brightness_level = settings.GetInt("brightness", 4);
    led_strip_->SetBrightness(LevelToBrightness(brightness_level), 4);

    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddTool("self.led_strip.set_brightness",
                       "Set the brightness of the LED ring (0-8). 0 = OFF, 8 = MAX.",
                       PropertyList({Property("level", kPropertyTypeInteger, 0, 8)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int level = properties["level"].value<int>();
                           ESP_LOGI(TAG, "Set LED brightness level to %d", level);
                           led_strip_->SetBrightness(LevelToBrightness(level), 4);
                           Settings s("led_strip", true);
                           s.SetInt("brightness", level);
                           return true;
                       });

    mcp_server.AddTool("self.led_strip.set_all_color",
                       "Set the color of all 8 LEDs. Values 0-255 for red, green, blue.",
                       PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                                     Property("green", kPropertyTypeInteger, 0, 255),
                                     Property("blue", kPropertyTypeInteger, 0, 255)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int r = properties["red"].value<int>();
                           int g = properties["green"].value<int>();
                           int b = properties["blue"].value<int>();
                           ESP_LOGI(TAG, "Set all LEDs to R=%d G=%d B=%d", r, g, b);
                           led_strip_->SetAllColor(RGBToColor(r, g, b));
                           return true;
                       });

    mcp_server.AddTool("self.led_strip.set_single_color",
                       "Set the color of a single LED by index (0-7).",
                       PropertyList({Property("index", kPropertyTypeInteger, 0, 7),
                                     Property("red", kPropertyTypeInteger, 0, 255),
                                     Property("green", kPropertyTypeInteger, 0, 255),
                                     Property("blue", kPropertyTypeInteger, 0, 255)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int idx = properties["index"].value<int>();
                           int r = properties["red"].value<int>();
                           int g = properties["green"].value<int>();
                           int b = properties["blue"].value<int>();
                           ESP_LOGI(TAG, "Set LED %d to R=%d G=%d B=%d", idx, r, g, b);
                           led_strip_->SetSingleColor(idx, RGBToColor(r, g, b));
                           return true;
                       });

    mcp_server.AddTool(
        "self.led_strip.blink", "Blink all LEDs with a given color and interval in milliseconds.",
        PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                      Property("green", kPropertyTypeInteger, 0, 255),
                      Property("blue", kPropertyTypeInteger, 0, 255),
                      Property("interval", kPropertyTypeInteger, 50, 2000)}),
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["red"].value<int>();
            int g = properties["green"].value<int>();
            int b = properties["blue"].value<int>();
            int interval = properties["interval"].value<int>();
            ESP_LOGI(TAG, "Blink LEDs R=%d G=%d B=%d interval=%dms", r, g, b, interval);
            led_strip_->Blink(RGBToColor(r, g, b), interval);
            return true;
        });

    mcp_server.AddTool("self.led_strip.scroll", "Scroll (marquee) effect on the LED ring.",
                       PropertyList({Property("red", kPropertyTypeInteger, 0, 255),
                                     Property("green", kPropertyTypeInteger, 0, 255),
                                     Property("blue", kPropertyTypeInteger, 0, 255),
                                     Property("length", kPropertyTypeInteger, 1, 7),
                                     Property("interval", kPropertyTypeInteger, 20, 1000)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int r = properties["red"].value<int>();
                           int g = properties["green"].value<int>();
                           int b = properties["blue"].value<int>();
                           int length = properties["length"].value<int>();
                           int interval = properties["interval"].value<int>();
                           ESP_LOGI(TAG, "Scroll LEDs R=%d G=%d B=%d len=%d interval=%dms", r, g, b,
                                    length, interval);
                           StripColor low = RGBToColor(4, 4, 4);
                           StripColor high = RGBToColor(r, g, b);
                           led_strip_->Scroll(low, high, length, interval);
                           return true;
                       });

    mcp_server.AddTool("self.led_strip.turn_off", "Turn off all LEDs.", PropertyList(),
                       [this](const PropertyList& properties) -> ReturnValue {
                           ESP_LOGI(TAG, "Turning off all LEDs");
                           led_strip_->SetAllColor(RGBToColor(0, 0, 0));
                           return true;
                       });
    ESP_LOGI(TAG, "Circular LED strip initialized");
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

void TEmbedBoard::InitI2c() {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = GPIO_NUM_8;
    bus_cfg.scl_io_num = GPIO_NUM_18;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &i2c_bus_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        i2c_bus_ = nullptr;
    } else {
        ESP_LOGI(TAG, "I2C bus initialized (SDA=8, SCL=18)");
    }
}

bool TEmbedBoard::ReadBatteryLevel() {
    if (!i2c_bus_)
        return false;

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = 0x6B;
    dev_cfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &dev) != ESP_OK) {
        ESP_LOGW(TAG, "BQ25896 not found on I2C");
        return false;
    }

     
    uint8_t reg = 0x0E;
    uint8_t val = 0;
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
         
        float vbat = 2.304f + (val & 0x7F) * 0.02f;
         
        int pct = (int)((vbat - 3.2f) / (4.2f - 3.2f) * 100.0f);
        if (pct < 0)
            pct = 0;
        if (pct > 100)
            pct = 100;
        battery_level_ = pct;
    }

     
    reg = 0x0B;
    val = 0;
    err = i2c_master_transmit_receive(dev, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        uint8_t vbus_stat = (val >> 5) & 0x07;   
        uint8_t chrg_stat = (val >> 3) & 0x03;   
         
         
        battery_charging_ = (vbus_stat != 0);
        ESP_LOGI(TAG, "BQ25896 REG0B=0x%02X vbus=%d chrg=%d", val, vbus_stat, chrg_stat);
    }

    i2c_master_bus_rm_device(dev);
    return true;
}

bool TEmbedBoard::GetBatteryLevel(int& level, bool& charging, bool& discharging) {
    ReadBatteryLevel();
    if (battery_level_ < 0)
        return false;
    level = battery_level_;
    charging = battery_charging_;
    discharging = !battery_charging_;
    return true;
}

std::string TEmbedBoard::GetDeviceStatusJson() {
    ReadBatteryLevel();
    char buf[128];
    if (battery_level_ >= 0) {
        snprintf(buf, sizeof(buf),
                 "{\"board_type\":\"t-embed\",\"battery_level\":%d,\"charging\":%s}",
                 battery_level_, battery_charging_ ? "true" : "false");
    } else {
        snprintf(buf, sizeof(buf), "{\"board_type\":\"t-embed\"}");
    }
    return std::string(buf);
}
