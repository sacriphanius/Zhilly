#ifndef _ES7210_AUDIO_CODEC_H
#define _ES7210_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <mutex>

class Es7210AudioCodec : public AudioCodec {
private:

    i2s_chan_handle_t rx_handle_ = nullptr;  
    i2s_chan_handle_t tx_handle_ = nullptr;  

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t i2c_dev_ = nullptr;
    bool owns_i2c_bus_ = false;

    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    bool pa_inverted_ = false;
    std::mutex mutex_;

    esp_err_t WriteReg(uint8_t reg, uint8_t val);
    uint8_t ReadReg(uint8_t reg);
    void UpdateRegBit(uint8_t reg, uint8_t mask, uint8_t data);

    void InitES7210(int sample_rate);
    void InitSpeakerI2S(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, int sample_rate);
    void InitMicI2S(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t lrck, gpio_num_t din,
                    int sample_rate);

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:

    Es7210AudioCodec(gpio_num_t i2c_sda, gpio_num_t i2c_scl, uint8_t es7210_addr,
                     gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout,
                     gpio_num_t mic_mclk, gpio_num_t mic_bclk, gpio_num_t mic_lrck,
                     gpio_num_t mic_din, int input_sample_rate, int output_sample_rate,
                     gpio_num_t pa_pin = GPIO_NUM_NC, bool pa_inverted = false);

    virtual ~Es7210AudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
};

#endif  
