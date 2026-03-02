#pragma once

#include "../lvgl_image.h"
#include "gifdec.h"
#include <lvgl.h>
#include <memory>
#include <functional>

class LvglGif {
public:
    explicit LvglGif(const lv_img_dsc_t* img_dsc);
    virtual ~LvglGif();

    virtual const lv_img_dsc_t* image_dsc() const;

    void Start();

    void Pause();

    void Resume();

    void Stop();

    bool IsPlaying() const;

    bool IsLoaded() const;

    int32_t GetLoopCount() const;

    void SetLoopCount(int32_t count);

    uint32_t GetLoopDelay() const;

    void SetLoopDelay(uint32_t delay_ms);

    uint16_t width() const;
    uint16_t height() const;

    void SetFrameCallback(std::function<void()> callback);

private:

    gd_GIF* gif_;

    lv_img_dsc_t img_dsc_;

    lv_timer_t* timer_;

    uint32_t last_call_;

    bool playing_;
    bool loaded_;

    uint32_t loop_delay_ms_;      
    bool loop_waiting_;           
    uint32_t loop_wait_start_;    

    std::function<void()> frame_callback_;

    void NextFrame();

    void Cleanup();
};
