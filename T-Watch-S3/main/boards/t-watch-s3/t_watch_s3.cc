#include "wifi_board.h"
#include "display/emote_display.h"
#include "audio/codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "axp2101.h"
#include "ir_service.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "TWatchS3"

class Cst816x : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    Cst816x(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA7);
        if (chip_id != 0) {
            ESP_LOGI(TAG, "Touch controller found at 0x%02X, Chip ID: 0x%02X", addr, chip_id);
        }
        read_buffer_ = new uint8_t[6];
    }

    ~Cst816x() {
        delete[] read_buffer_;
    }

    void UpdateTouchPoint() {
        ReadRegs(0x02, read_buffer_, 6);
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    const TouchPoint_t &GetTouchPoint() {
        return tp_;
    }

private:
    uint8_t *read_buffer_ = nullptr;
    TouchPoint_t tp_;
};

class TWatchS3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_bus_handle_t touch_bus_;
    Display* display_;
    Button boot_button_;
    Axp2101* pmu_;
    Cst816x* touch_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    int64_t last_activity_time_ = 0;
    bool display_dimmed_ = false;

    void InitializeI2c() {
        ESP_LOGI(TAG, "Initialize I2C bus (Sensors)");
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        ESP_LOGI(TAG, "Initialize I2C bus (Touch)");
        i2c_master_bus_config_t touch_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = I2C_TOUCH_SDA_PIN,
            .scl_io_num = I2C_TOUCH_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&touch_bus_cfg, &touch_bus_));
    }

    void InitializePmu() {
        ESP_LOGI(TAG, "Initialize AXP2101 PMU");
        pmu_ = new Axp2101(i2c_bus_, AXP2101_ADDR);

        pmu_->WriteReg(0x10, 0x00);
        pmu_->WriteReg(0x16, 0x08);
        pmu_->WriteReg(0x31, 0x00);

        pmu_->WriteReg(0x90, 0xBF);

        pmu_->WriteReg(0x91, 0x1C);
        pmu_->WriteReg(0x92, 0x1C);
        pmu_->WriteReg(0x93, 0x1C);
        pmu_->WriteReg(0x94, 0x1C);
        pmu_->WriteReg(0x95, 0x1C);
        pmu_->WriteReg(0x96, 0x1C);
        pmu_->WriteReg(0x99, 0x1C);

        ESP_LOGI(TAG, "AXP2101 power rails configured and enabled");
    }

    void InitializeTouch() {
        ESP_LOGI(TAG, "Scanning touch controller...");
        vTaskDelay(pdMS_TO_TICKS(100));

        touch_ = new Cst816x(touch_bus_, 0x15);
        if (touch_->ReadReg(0xA7) == 0) {
            delete touch_;
            ESP_LOGI(TAG, "Touch not found at 0x15, trying 0x38 (FT6X36)...");
            touch_ = new Cst816x(touch_bus_, 0x38);
        }

        last_activity_time_ = esp_timer_get_time();
        xTaskCreate(touchpad_daemon, "touch", 4096, this, 5, NULL);
    }

    static void touchpad_daemon(void *param) {
        TWatchS3Board* board = (TWatchS3Board*)param;
        auto touch = board->touch_;
        if (touch == nullptr) {
            ESP_LOGE(TAG, "Touch controller not initialized, exiting daemon");
            vTaskDelete(NULL);
            return;
        }
        bool was_touched = false;
        while (true) {
            touch->UpdateTouchPoint();

            auto& app = Application::GetInstance();
            DeviceState state = app.GetDeviceState();
            if (state != kDeviceStateIdle) {
                board->last_activity_time_ = esp_timer_get_time();
            }

            if (touch->GetTouchPoint().num > 0) {
                board->last_activity_time_ = esp_timer_get_time();
                if (board->display_dimmed_) {
                    board->display_dimmed_ = false;
                    board->GetBacklight()->RestoreBrightness();
                    ESP_LOGI(TAG, "Display woken up by touch");
                }

                if (!was_touched) {
                    was_touched = true;
                    ESP_LOGI(TAG, "Touch detected, toggling chat state");
                    app.ToggleChatState();
                }
            } else {
                was_touched = false;

                if (!board->display_dimmed_ && (esp_timer_get_time() - board->last_activity_time_) > 15000000) {
                    board->display_dimmed_ = true;
                    board->GetBacklight()->SetBrightness(0);
                    ESP_LOGI(TAG, "Display dimmed due to 15s inactivity");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "Initialize ST7789 display with Emote support");

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install ST7789 panel driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new emote::EmoteDisplay(panel_, panel_io_, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            last_activity_time_ = esp_timer_get_time();
            if (display_dimmed_) {
                display_dimmed_ = false;
                GetBacklight()->RestoreBrightness();
                ESP_LOGI(TAG, "Display woken up by button");
                return;
            }

            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    TWatchS3Board() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "Initializing T-Watch S3 Board...");
        InitializeI2c();
        ESP_LOGI(TAG, "I2C initialized");
        InitializePmu();
        ESP_LOGI(TAG, "PMU initialized");
        InitializeTouch();
        ESP_LOGI(TAG, "Touch initialized");
        InitializeSpi();
        ESP_LOGI(TAG, "SPI initialized");
        InitializeSt7789Display();
        ESP_LOGI(TAG, "Display initialized");
        InitializeButtons();
        ESP_LOGI(TAG, "Buttons initialized");
        GetBacklight()->RestoreBrightness();
        IrService::GetInstance().Initialize(IR_TX_PIN);
        ESP_LOGI(TAG, "Board initialization complete");
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_CLK,
            AUDIO_I2S_MIC_GPIO_DATA);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        level = pmu_->GetBatteryLevel();
        charging = pmu_->IsCharging();
        discharging = pmu_->IsDischarging();
        return true;
    }
};

DECLARE_BOARD(TWatchS3Board);