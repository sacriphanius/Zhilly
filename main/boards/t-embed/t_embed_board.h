#ifndef T_EMBED_BOARD_H
#define T_EMBED_BOARD_H

#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include "audio/audio_codec.h"
#include "audio/codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/display.h"
#include "wifi_board.h"

#include "boards/common/backlight.h"
#include "driver/i2c_master.h"
#include "led/circular_strip.h"
#include "mcp_server.h"

class TEmbedBoard : public WifiBoard {
private:
    Display* display_;
    Backlight* backlight_;
    Button boot_button_;
    Button pwr_button_;
    CircularStrip* led_strip_ = nullptr;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    int battery_level_ = -1;
    bool battery_charging_ = false;

    void InitSpi();
    void InitLcdDisplay();
    void InitializeButtons();
    void InitializeSdCard();
    void InitLedStrip();
    void InitI2c();
    bool ReadBatteryLevel();

public:
    TEmbedBoard();
    virtual std::string GetBoardType() override { return "t-embed"; }
    virtual std::string GetBoardJson() override { return "{\"type\":\"t-embed\"}"; }
    virtual std::string GetDeviceStatusJson() override;
    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override;
    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
};

#endif
