#pragma once

#include <mutex>
#include <vector>
#include <string>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "boards/t-watch-s3/config.h"

class RadioService {
public:
    static RadioService& GetInstance();
    bool Initialize();
    bool TransmitTeslaPortSignal();
    bool StartJammer(uint32_t freq, uint32_t duration_ms);
    bool StopJammer();
    bool IsJamming() const { return is_jamming_; }
    std::string GetInfo();

private:
    static void JammerTask(void* arg);

    RadioService();
    ~RadioService();

    spi_device_handle_t spi_handle_ = nullptr;
    std::mutex spi_mutex_;
    bool initialized_ = false;
    bool is_jamming_ = false;
    TaskHandle_t jammer_task_handle_ = nullptr;
    esp_timer_handle_t jammer_timer_ = nullptr;

    // SX1262 Low-level methods
    void WriteCommand(uint8_t opcode, const uint8_t* data, size_t len);
    void ReadCommand(uint8_t opcode, uint8_t* data, size_t len);
    void WaitBusy();
    void Reset();
    
    // SX1262 Radio methods
    void SetStandby(bool xosc = false);
    void SetTxParams(int8_t power);
    void SetRfFrequency(uint32_t frequency);
    void SetPaConfig();
    void SetTxContinuousWave();
};
