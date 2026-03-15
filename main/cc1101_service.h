#ifndef CC1101_SERVICE_H
#define CC1101_SERVICE_H

#define CC1101_MDMCFG4 0x10
#define CC1101_MDMCFG3 0x11
#define CC1101_MDMCFG2 0x12
#define CC1101_MDMCFG1 0x13
#define CC1101_MDMCFG0 0x14

#include <driver/gpio.h>
#include <driver/rmt_tx.h>
#include <driver/spi_master.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

struct Cc1101RegisterSetting {
    uint8_t reg;
    uint8_t val;
};

struct Cc1101Preset {
    std::string name;
    std::string modulation;
    float frequency;
    std::vector<Cc1101RegisterSetting> registers;
};

// Parsed data from a Flipper-compatible .sub file
struct SubFileData {
    float       frequency   = 433920000.0f; // Hz
    std::string protocol    = "";           // RAW, BinRAW, RcSwitch, Princeton, CAME, ...
    std::string preset      = "";           // e.g. FuriHalSubGhzPresetOok270Async
    int         te          = 0;            // TE pulse width in µs
    int         bit         = 0;            // Number of bits
    uint64_t    key         = 0;            // Decoded key value (hex)
    std::string bin_data    = "";           // ASCII binary string for BinRAW
    std::vector<int32_t> raw_samples;       // RAW_Data timings (µs, signed)
};

// Protocol table entry for known named protocols (CAME, Princeton, Holtek...)
struct ProtocolTableEntry {
    const char* name;          // Protocol name as it appears in .sub file
    int pilot_high_te;         // pilot burst HIGH in TE multiples
    int pilot_low_te;          // pilot burst LOW in TE multiples
    int zero_high_te;          // '0' bit HIGH
    int zero_low_te;           // '0' bit LOW
    int one_high_te;           // '1' bit HIGH
    int one_low_te;            // '1' bit LOW
    int stop_high_te;          // stop bit HIGH (0 = none)
    int stop_low_te;           // stop bit LOW  (0 = none)
    bool msb_first;            // bit order
};

class Cc1101Service {
public:
    Cc1101Service();
    ~Cc1101Service();

    bool Init();
    bool Deinit();
    bool SetFrequency(float mhz);
    bool SetModulation(const std::string& type);
    float ReadRssi();
    bool GetStatus();
    uint8_t GetChipVersion();
    void DumpRegisters();

    bool LoadPresets(const std::string& path);
    bool ApplyPreset(const std::string& preset_name);
    bool ApplyFlipperPreset(const std::string& preset_name);

    std::string ListSdFiles() const;
    std::string ListSdSubFiles() const;

    bool ReplaySubFile(const std::string& filename);
    bool StopReplay();
    bool IsReplaying() const { return is_replaying_; }

    // Faz 4: Save captured RAW signal to .sub file on SD card
    bool SaveRawToSubFile(const std::vector<int32_t>& samples, float freq_mhz,
                          const std::string& filepath = "");

    // Internal helpers exposed for lambda/task use
    void _SetReplaying(bool state);
    void _CommandStrobe(uint8_t cmd);
    const std::vector<int32_t>& _GetRawBuffer() const { return replay_buffer_; }
    const SubFileData& _GetSubData() const { return sub_data_; }
    const std::string& _GetReplayPath() const { return current_sub_path_; }
    float _GetFrequency() const { return current_frequency_mhz_; }
    const std::string& _GetCurrentModulation() const { return current_modulation_; }
    void _WriteRegPub(uint8_t a, uint8_t d) { WriteReg(a, d); }
    void WriteBurstReg(uint8_t addr, const uint8_t* buffer, uint8_t num);
    void _SetJammingTaskHandle(TaskHandle_t h) { jammer_task_handle_ = h; }

    bool StartJammer(uint32_t duration_ms);
    bool StopJammer();
    bool IsJamming() const { return is_jamming_; }

    // Hardcoded special transmissions
    bool TransmitTeslaPortSignal();

private:
    spi_device_handle_t  spi_handle_       = nullptr;
    rmt_channel_handle_t rmt_tx_channel_   = nullptr;
    rmt_encoder_handle_t rmt_encoder_      = nullptr;
    bool initialized_                      = false;
    float current_frequency_mhz_           = 433.92f;
    std::string current_modulation_        = "ASK";

    volatile bool is_jamming_               = false;
    TaskHandle_t jammer_task_handle_       = nullptr;
    esp_timer_handle_t jammer_timer_        = nullptr;

    volatile bool is_replaying_             = false;
    TaskHandle_t replay_task_handle_       = nullptr;
    std::string current_sub_path_;
    std::vector<int32_t> replay_buffer_;   // RAW samples buffer (for task)
    SubFileData sub_data_;                 // Fully parsed sub file data

    std::map<std::string, Cc1101Preset> presets_;

    void WriteReg(uint8_t addr, uint8_t data);
    uint8_t ReadReg(uint8_t addr);
    void CommandStrobe(uint8_t cmd);
    void Reset();
    void SetRfSettings();

    // .sub file helpers
    bool ParseSubFile(const std::string& path, SubFileData& out);
    bool TransmitRaw(const SubFileData& data);
    bool TransmitBinRaw(const SubFileData& data);
    bool TransmitKey(const SubFileData& data);
    bool TransmitProtocol(const SubFileData& data); // Faz 5: named protocol tables

    std::string FindFileCaseInsensitive(const std::string& directory,
                                        const std::string& filename);
    std::string GenerateSubFilename() const;         // Faz 4: auto-name helper

    std::mutex spi_mutex_;
};

#endif
