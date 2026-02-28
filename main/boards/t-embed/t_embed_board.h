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

class TEmbedBoard : public WifiBoard {
private:
    Display* display_;
    Backlight* backlight_;
    Button boot_button_;

    void InitSpi();
    void InitLcdDisplay();
    void InitializeButtons();
    void InitializeSdCard();

public:
    TEmbedBoard();
    virtual std::string GetBoardType() override { return "t-embed"; }
    virtual std::string GetBoardJson() override { return "{\"type\":\"t-embed\"}"; }
    virtual std::string GetDeviceStatusJson() override { return "{\"board_type\":\"t-embed\"}"; }
    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
};

#endif  // T_EMBED_BOARD_H
