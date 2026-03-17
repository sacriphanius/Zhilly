#ifndef IR_SERVICE_H
#define IR_SERVICE_H

#include <driver/rmt_tx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>

enum IrJammerMode {
    kIrJammerModeBasic,
    kIrJammerModeEnhanced,
    kIrJammerModeSweep,
    kIrJammerModeRandom,
    kIrJammerModeEmpty
};

class IrService {
public:
    static IrService& GetInstance();
    void Initialize(int tx_pin);
    
    // TV-B-Gone
    void StartTvBGone(const std::string& region); // "NA" or "EU"
    
    // Jammer
    void StartJammer(IrJammerMode mode);
    
    // Control
    void Stop();
    bool IsRunning() const { return running_; }

private:
    IrService();
    ~IrService();
    
    int tx_pin_ = -1;
    rmt_channel_handle_t tx_chan_ = nullptr;
    rmt_encoder_handle_t raw_encoder_ = nullptr;
    
    TaskHandle_t task_handle_ = nullptr;
    volatile bool running_ = false;
    volatile bool stop_requested_ = false;
    
    static void TaskWrapper(void* arg);
    void RunTvBGone(const std::string& region);
    void RunJammer(IrJammerMode mode);
    
    void SendRaw(const uint32_t* durations, size_t count, uint32_t freq_hz);
};

#endif
