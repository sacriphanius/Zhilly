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

    std::string ListSdFiles() const;

    bool ReplaySubFile(const std::string& filename);
    bool StopReplay();
    bool IsReplaying() const { return is_replaying_; }
    void _SetReplaying(bool state);
    void _CommandStrobe(uint8_t cmd);
    const std::vector<int32_t>& _GetRawBuffer() const { return replay_buffer_; }
    const std::string& _GetReplayPath() const { return current_sub_path_; }
    float _GetFrequency() const { return current_frequency_mhz_; }
    const std::string& _GetCurrentModulation() const { return current_modulation_; }
    void _WriteRegPub(uint8_t a, uint8_t d) { WriteReg(a, d); }

    bool StartJammer(uint32_t duration_ms);
    bool StopJammer();
    bool IsJamming() const { return is_jamming_; }

private:
    spi_device_handle_t spi_handle_ = nullptr;
    rmt_channel_handle_t rmt_tx_channel_ = nullptr;
    rmt_encoder_handle_t rmt_encoder_ = nullptr;
    bool initialized_ = false;
    float current_frequency_mhz_ = 433.92f;
    std::string current_modulation_ = "ASK";

    bool is_jamming_ = false;
    void* jammer_task_handle_ = nullptr;

    bool is_replaying_ = false;
    void* replay_task_handle_ = nullptr;
    std::string current_sub_path_;
    std::vector<int32_t> replay_buffer_;

    std::map<std::string, Cc1101Preset> presets_;

    void WriteReg(uint8_t addr, uint8_t data);
    uint8_t ReadReg(uint8_t addr);
    void CommandStrobe(uint8_t cmd);
    void Reset();
    void SetRfSettings();
    std::string FindFileCaseInsensitive(const std::string& directory, const std::string& filename);
};

#endif
