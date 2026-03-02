#ifndef CC1101_SERVICE_H
#define CC1101_SERVICE_H

#define CC1101_MDMCFG4 0x10   
#define CC1101_MDMCFG3 0x11   
#define CC1101_MDMCFG2 0x12   
#define CC1101_MDMCFG1 0x13   
#define CC1101_MDMCFG0 0x14   

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <cstdint>
#include <string>
#include <vector>

struct CapturedSample {
    int64_t timestamp_ms;
    float rssi_dbm;
};

class Cc1101Service {
public:
    Cc1101Service();
    ~Cc1101Service();

    bool Init();
    bool SetFrequency(float mhz);
    bool SetModulation(const std::string& type);
    bool RxStart(uint32_t timeout_ms);
    bool RxStop();
    float ReadRssi();
    bool GetStatus();

    void ClearCapture();
    void RecordSample();
    size_t GetCaptureCount() const;
    std::string GetCaptureSummary() const;
    bool SaveCaptureToSd(const std::string& filename);

     
    bool StartJammer(uint32_t duration_ms);
    bool StopJammer();
    bool IsJamming() const { return is_jamming_; }

     
    bool StartRawCapture(uint32_t timeout_ms = 0);
    bool StopRawCapture();
    bool SaveSubFile(const std::string& filename) const;
    bool ReplaySubFile(const std::string& filename);
    std::string ListSubFiles() const;
    bool IsRawCapturing() const { return is_raw_capturing_; }
    bool IsReplaying() const { return is_replaying_; }
    size_t GetRawCaptureCount() const { return raw_capture_buffer_.size(); }
    void _PushRawSample(int32_t val);
    void _CommandStrobe(uint8_t cmd);
    void _SetReplaying(bool state);
    const std::vector<int32_t>& _GetRawBuffer() const { return raw_capture_buffer_; }

private:
    spi_device_handle_t spi_handle_ = nullptr;
    bool initialized_ = false;
    bool rx_active_ = false;
    float current_frequency_mhz_ = 433.92f;
    std::string current_modulation_ = "ASK";

    std::vector<CapturedSample> capture_buffer_;
    int64_t rx_start_time_ms_ = 0;

     
    bool is_jamming_ = false;
    void* jammer_task_handle_ = nullptr;

     
    bool is_raw_capturing_ = false;
    bool is_replaying_ = false;
    std::vector<int32_t> raw_capture_buffer_;   
    void* raw_capture_task_handle_ = nullptr;
    void* replay_task_handle_ = nullptr;
    std::string current_replay_file_;

    void WriteReg(uint8_t addr, uint8_t data);
    uint8_t ReadReg(uint8_t addr);
    void CommandStrobe(uint8_t cmd);
    void Reset();
    void SetRfSettings();
};

#endif   
