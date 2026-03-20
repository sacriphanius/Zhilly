#ifndef T_EMBED_BOARD_H
#define T_EMBED_BOARD_H

#include "audio/codecs/no_audio_codec.h"
#include "button.h"
#include "config.h"
#include "display/lcd_display.h"
#include "wifi_board.h"

class TEmbedBoard : public WifiBoard {
private:
    LcdDisplay* display_;
    Button boot_button_;
    Button user_button_;

    void InitSpi();
    void InitLcdDisplay();
    void InitializeButtons();

public:
    TEmbedBoard();
    virtual std::string GetBoardType() override { return "m5stack-cardputer-adv"; }
    virtual std::string GetBoardJson() override { return "{\"type\":\"m5stack-cardputer-adv\"}"; }
    virtual std::string GetDeviceStatusJson() override {
        return "{\"board_type\":\"m5stack-cardputer-adv\"}";
    }
    virtual AudioCodec* GetAudioCodec() override;
    virtual Display* GetDisplay() override;
    virtual Backlight* GetBacklight() override;
};

#endif
