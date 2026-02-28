#include "es7210_audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <esp_log.h>
#include <cmath>
#include <cstring>

#define TAG "Es7210AudioCodec"

// ── ES7210 Register definitions ──────────────────────────────────────────────
#define ES7210_RESET_REG00 0x00
#define ES7210_CLOCK_OFF_REG01 0x01
#define ES7210_MAINCLK_REG02 0x02
#define ES7210_LRCK_DIVH_REG04 0x04
#define ES7210_LRCK_DIVL_REG05 0x05
#define ES7210_POWER_DOWN_REG06 0x06
#define ES7210_OSR_REG07 0x07
#define ES7210_TIME_CTRL0_REG09 0x09
#define ES7210_TIME_CTRL1_REG0A 0x0A
#define ES7210_SDP_IFACE1_REG11 0x11
#define ES7210_ANALOG_REG40 0x40
#define ES7210_MIC12_BIAS_REG41 0x41
#define ES7210_MIC34_BIAS_REG42 0x42
#define ES7210_MIC1_GAIN_REG43 0x43
#define ES7210_MIC2_GAIN_REG44 0x44
#define ES7210_MIC1_POWER_REG47 0x47
#define ES7210_MIC2_POWER_REG48 0x48
#define ES7210_MIC3_POWER_REG49 0x49
#define ES7210_MIC4_POWER_REG4A 0x4A
#define ES7210_MIC12_POWER_REG4B 0x4B
#define ES7210_MIC34_POWER_REG4C 0x4C

#define AUDIO_CODEC_DMA_DESC_NUM 6
#define AUDIO_CODEC_DMA_FRAME_NUM 240

// ── I2C Helpers ───────────────────────────────────────────────────────────────

esp_err_t Es7210AudioCodec::WriteReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(i2c_dev_, buf, 2, pdMS_TO_TICKS(100));
}

uint8_t Es7210AudioCodec::ReadReg(uint8_t reg) {
    uint8_t result = 0;
    i2c_master_transmit_receive(i2c_dev_, &reg, 1, &result, 1, pdMS_TO_TICKS(100));
    return result;
}

void Es7210AudioCodec::UpdateRegBit(uint8_t reg, uint8_t mask, uint8_t data) {
    uint8_t val = ReadReg(reg);
    val = (val & ~mask) | (mask & data);
    WriteReg(reg, val);
}

// ── ES7210 Init ───────────────────────────────────────────────────────────────

void Es7210AudioCodec::InitES7210(int sample_rate) {
    // Reset
    WriteReg(ES7210_RESET_REG00, 0xFF);
    vTaskDelay(pdMS_TO_TICKS(10));
    WriteReg(ES7210_RESET_REG00, 0x41);

    WriteReg(ES7210_CLOCK_OFF_REG01, 0x1F);
    WriteReg(ES7210_TIME_CTRL0_REG09, 0x30);
    WriteReg(ES7210_TIME_CTRL1_REG0A, 0x30);

    // Analog power, VMID=5kΩ
    WriteReg(ES7210_ANALOG_REG40, 0xC3);
    WriteReg(ES7210_MIC12_BIAS_REG41, 0x70);  // 2.87V bias
    WriteReg(ES7210_MIC34_BIAS_REG42, 0x70);

    // OSR & clock divider for 16kHz  (MCLK = 16000 * 256 = 4096000)
    // Using coeff: mclk=4096000, lrck=16000 → adc_div=1, dll=1, osr=0x20
    WriteReg(ES7210_OSR_REG07, 0x20);
    WriteReg(ES7210_MAINCLK_REG02, 0x81);  // adc_div=1, dll bypass=1
    WriteReg(ES7210_LRCK_DIVH_REG04, 0x01);
    WriteReg(ES7210_LRCK_DIVL_REG05, 0x00);

    // I2S format: 16-bit, I2S standard
    WriteReg(ES7210_SDP_IFACE1_REG11, 0x60);  // 16-bit I2S

    // Enable MIC1 & MIC2 only (MIC3/4 powered down)
    WriteReg(ES7210_MIC12_POWER_REG4B, 0x00);  // power on MIC1&2
    WriteReg(ES7210_MIC34_POWER_REG4C, 0xFF);  // power off MIC3&4
    WriteReg(ES7210_MIC1_POWER_REG47, 0x00);
    WriteReg(ES7210_MIC2_POWER_REG48, 0x00);
    WriteReg(ES7210_MIC3_POWER_REG49, 0xFF);
    WriteReg(ES7210_MIC4_POWER_REG4A, 0xFF);

    // Enable MIC1/2 channels and set gain (~24dB = index 8)
    UpdateRegBit(ES7210_CLOCK_OFF_REG01, 0x0B, 0x00);  // enable MIC1&2 clocks
    UpdateRegBit(ES7210_MIC1_GAIN_REG43, 0x1F, 0x18);  // enable + 24dB
    UpdateRegBit(ES7210_MIC2_GAIN_REG44, 0x1F, 0x18);

    // Start ADC
    WriteReg(ES7210_CLOCK_OFF_REG01, 0x00);
    WriteReg(ES7210_POWER_DOWN_REG06, 0x00);

    ESP_LOGI(TAG, "ES7210 initialized at %dHz", sample_rate);
}

// ── I2S Speaker (TX) ──────────────────────────────────────────────────────────

void Es7210AudioCodec::InitSpeakerI2S(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,
                                      int sample_rate) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)1, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr));

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)sample_rate,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_LEFT,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
            },
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Speaker I2S (port 1) initialized: BCLK=%d WS=%d DOUT=%d", bclk, ws, dout);
}

// ── I2S Mic (RX) ─────────────────────────────────────────────────────────────

void Es7210AudioCodec::InitMicI2S(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t lrck, gpio_num_t din,
                                  int sample_rate) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)sample_rate,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_MONO,
                .slot_mask = I2S_STD_SLOT_LEFT,
                .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol = false,
                .bit_shift = true,
            },
        .gpio_cfg =
            {
                .mclk = mclk,
                .bclk = bclk,
                .ws = lrck,
                .dout = I2S_GPIO_UNUSED,
                .din = din,
                .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false},
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Mic I2S (port 0) initialized: MCLK=%d BCLK=%d LRCK=%d DIN=%d", mclk, bclk, lrck,
             din);
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

Es7210AudioCodec::Es7210AudioCodec(gpio_num_t i2c_sda, gpio_num_t i2c_scl, uint8_t es7210_addr,
                                   gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout,
                                   gpio_num_t mic_mclk, gpio_num_t mic_bclk, gpio_num_t mic_lrck,
                                   gpio_num_t mic_din, int input_sample_rate,
                                   int output_sample_rate, gpio_num_t pa_pin, bool pa_inverted) {
    duplex_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    pa_pin_ = pa_pin;
    pa_inverted_ = pa_inverted;
    input_channels_ = 1;
    input_reference_ = false;
    input_gain_ = 40.0f;  // initial software gain dB

    // Init I2C bus
    i2c_master_bus_config_t i2c_bus_cfg = {};
    i2c_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_cfg.i2c_port = I2C_NUM_0;
    i2c_bus_cfg.scl_io_num = i2c_scl;
    i2c_bus_cfg.sda_io_num = i2c_sda;
    i2c_bus_cfg.glitch_ignore_cnt = 7;
    i2c_bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    owns_i2c_bus_ = true;

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = es7210_addr;
    dev_cfg.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &i2c_dev_));

    // Hardware init
    InitES7210(input_sample_rate);
    InitSpeakerI2S(spk_bclk, spk_ws, spk_dout, output_sample_rate);
    InitMicI2S(mic_mclk, mic_bclk, mic_lrck, mic_din, input_sample_rate);

    // PA pin setup
    if (pa_pin_ != GPIO_NUM_NC) {
        gpio_config_t io_cfg = {};
        io_cfg.pin_bit_mask = (1ULL << pa_pin_);
        io_cfg.mode = GPIO_MODE_OUTPUT;
        gpio_config(&io_cfg);
        gpio_set_level(pa_pin_, pa_inverted_ ? 1 : 0);  // default off
    }

    ESP_LOGI(TAG, "Es7210AudioCodec ready");
}

Es7210AudioCodec::~Es7210AudioCodec() {
    if (rx_handle_) {
        i2s_channel_disable(rx_handle_);
        i2s_del_channel(rx_handle_);
    }
    if (tx_handle_) {
        i2s_channel_disable(tx_handle_);
        i2s_del_channel(tx_handle_);
    }
    if (i2c_dev_) {
        i2c_master_bus_rm_device(i2c_dev_);
    }
    if (i2c_bus_ && owns_i2c_bus_) {
        i2c_del_master_bus(i2c_bus_);
    }
}

// ── AudioCodec overrides ──────────────────────────────────────────────────────

void Es7210AudioCodec::SetOutputVolume(int volume) { AudioCodec::SetOutputVolume(volume); }

void Es7210AudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enable == input_enabled_)
        return;
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
    }
    AudioCodec::EnableInput(enable);
}

void Es7210AudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enable == output_enabled_)
        return;
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
    }
    if (pa_pin_ != GPIO_NUM_NC) {
        int lvl = enable ? 1 : 0;
        gpio_set_level(pa_pin_, pa_inverted_ ? !lvl : lvl);
    }
    AudioCodec::EnableOutput(enable);
}

int Es7210AudioCodec::Read(int16_t* dest, int samples) {
    size_t bytes_read = 0;
    if (i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY) !=
        ESP_OK) {
        ESP_LOGE(TAG, "Mic read failed");
        return 0;
    }
    int got = (int)(bytes_read / sizeof(int16_t));

    // Apply software gain if set
    if (input_gain_ > 0.0f) {
        int32_t scale = (int32_t)input_gain_;
        for (int i = 0; i < got; i++) {
            int32_t v = (int32_t)dest[i] * scale;
            dest[i] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
        }
    }
    return got;
}

int Es7210AudioCodec::Write(const int16_t* data, int samples) {
    // Speaker: I2S expects 32-bit samples; apply volume scaling
    std::vector<int32_t> buf(samples);
    int32_t vol_factor = (int32_t)(std::pow((double)output_volume_ / 100.0, 2) * 65536);
    for (int i = 0; i < samples; i++) {
        int64_t tmp = (int64_t)data[i] * vol_factor;
        buf[i] = (tmp > INT32_MAX) ? INT32_MAX : (tmp < INT32_MIN) ? INT32_MIN : (int32_t)tmp;
    }
    size_t written = 0;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle_, buf.data(), samples * sizeof(int32_t), &written,
                                      portMAX_DELAY));
    return (int)(written / sizeof(int32_t));
}
