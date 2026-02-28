#ifndef _ES7210_AUDIO_CODEC_H
#define _ES7210_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <mutex>

/**
 * ES7210 Audio Codec — Sadece ADC (mikrofon girişi) destekler.
 * Hoparlör çıkışı için ayrı bir I2S kanalı (NoAudioCodec TX) kullanılır.
 * T-Embed CC1101 konfigürasyonu:
 *   Mic  I2S: BCLK=47, LRCK=21, DIN=14, MCLK=48
 *   Mic  I2C: SDA=18, SCL=8, addr=0x40
 *   SPK  I2S: BCLK=7,  WS=5,  DOUT=6
 */
class Es7210AudioCodec : public AudioCodec {
private:
    // I2S handles
    i2s_chan_handle_t rx_handle_ = nullptr;  // mic input
    i2s_chan_handle_t tx_handle_ = nullptr;  // speaker output

    // I2C for ES7210 register control
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t i2c_dev_ = nullptr;
    bool owns_i2c_bus_ = false;

    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    bool pa_inverted_ = false;
    std::mutex mutex_;

    // ES7210 register helpers
    esp_err_t WriteReg(uint8_t reg, uint8_t val);
    uint8_t ReadReg(uint8_t reg);
    void UpdateRegBit(uint8_t reg, uint8_t mask, uint8_t data);

    // Codec init helpers
    void InitES7210(int sample_rate);
    void InitSpeakerI2S(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, int sample_rate);
    void InitMicI2S(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t lrck, gpio_num_t din,
                    int sample_rate);

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    /**
     * @param i2c_sda   SDA pin for ES7210
     * @param i2c_scl   SCL pin for ES7210
     * @param es7210_addr  I2C address (default 0x40)
     * @param spk_bclk  Speaker I2S bit clock
     * @param spk_ws    Speaker I2S word select (LR clock)
     * @param spk_dout  Speaker I2S data out
     * @param mic_mclk  Mic master clock
     * @param mic_bclk  Mic bit clock
     * @param mic_lrck  Mic LR clock
     * @param mic_din   Mic data in
     * @param input_sample_rate   Mic sample rate (Hz)
     * @param output_sample_rate  Speaker sample rate (Hz)
     * @param pa_pin    PA enable pin (or GPIO_NUM_NC)
     * @param pa_inverted true if PA enable is active-low
     */
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

#endif  // _ES7210_AUDIO_CODEC_H
