#include "ir_service.h"
#include "WORLD_IR_CODES.h"
#include <esp_log.h>
#include <cmath>
#include <algorithm>
#include <string>

#define TAG "IrService"

IrService& IrService::GetInstance() {
    static IrService instance;
    return instance;
}

IrService::IrService() {}

IrService::~IrService() {
    Stop();
}

void IrService::Initialize(int tx_pin) {
    tx_pin_ = tx_pin;
    
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num = (gpio_num_t)tx_pin_,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000, // 1us resolution
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .intr_priority = 0,
        .flags = {
            .invert_out = 0,
            .with_dma = 0,
            .io_loop_back = 0,
            .io_od_mode = 0,
        },
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_chan_));
    
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000,
        .duty_cycle = 0.33,
        .flags = {
            .polarity_active_low = 0,
        },
    };
    ESP_ERROR_CHECK(rmt_apply_carrier(tx_chan_, &carrier_cfg));
    
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_encoder_config, &raw_encoder_));
    
    ESP_ERROR_CHECK(rmt_enable(tx_chan_));
}

struct TaskParams {
    IrService* service;
    std::string region_or_mode;
};

void IrService::StartTvBGone(const std::string& region) {
    Stop();
    running_ = true;
    TaskParams* params = new TaskParams{this, region};
    xTaskCreate(TaskWrapper, "tvbgone_task", 4096, params, 5, &task_handle_);
}

void IrService::StartJammer(IrJammerMode mode) {
    Stop();
    running_ = true;
    TaskParams* params = new TaskParams{this, std::to_string((int)mode)};
    xTaskCreate(TaskWrapper, "jammer_task", 4096, params, 5, &task_handle_);
}

void IrService::Stop() {
    running_ = false;
    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(150));
        task_handle_ = nullptr;
    }
}

void IrService::TaskWrapper(void* arg) {
    TaskParams* params = static_cast<TaskParams*>(arg);
    IrService* service = params->service;
    std::string data = params->region_or_mode;
    
    if (data.compare("NA") == 0 || data.compare("EU") == 0) {
        service->RunTvBGone(data);
    } else {
        int mode_int = std::stoi(data);
        service->RunJammer((IrJammerMode)mode_int);
    }
    
    delete params;
    service->running_ = false;
    service->task_handle_ = nullptr;
    vTaskDelete(NULL);
}

void IrService::RunTvBGone(const std::string& region) {
    const IrCode* const* codes = (region.compare("NA") == 0) ? NApowerCodes : EUpowerCodes;
    int max_idx = (region.compare("NA") == 0) ? 250 : 150; 

    for (int i = 0; i < max_idx && running_; ++i) {
        const IrCode* code = codes[i];
        if (code == nullptr) break;
        
        uint32_t freq = code->timer_val * 1000;
        rmt_carrier_config_t carrier_cfg = {
            .frequency_hz = freq,
            .duty_cycle = 0.33,
            .flags = { .polarity_active_low = 0 },
        };
        rmt_apply_carrier(tx_chan_, &carrier_cfg);
        
        std::vector<uint32_t> durations;
        uint8_t bitsleft = 0;
        uint8_t bits = 0;
        int code_ptr = 0;
        
        for (int k = 0; k < code->numpairs; k++) {
            uint8_t index = 0;
            for (int b = 0; b < code->bitcompression; b++) {
                if (bitsleft == 0) {
                    bits = code->codes[code_ptr++];
                    bitsleft = 8;
                }
                index = (index << 1) | ((bits >> --bitsleft) & 1);
            }
            
            durations.push_back(code->times[index * 2] * 10);
            durations.push_back(code->times[index * 2 + 1] * 10);
        }
        
        rmt_transmit_config_t transmit_config = { .loop_count = 0, .flags = { .eot_level = 0 } };
        rmt_transmit(tx_chan_, raw_encoder_, durations.data(), durations.size() * sizeof(uint32_t), &transmit_config);
        rmt_tx_wait_all_done(tx_chan_, -1);
        
        vTaskDelay(pdMS_TO_TICKS(205));
    }
}

void IrService::RunJammer(IrJammerMode mode) {
    uint32_t freqs[] = {30000, 33000, 36000, 38000, 40000, 42000, 56000};
    int freq_idx = 3; // 38kHz
    
    while (running_) {
        uint32_t freq = freqs[freq_idx];
        rmt_carrier_config_t carrier_cfg = {
            .frequency_hz = freq,
            .duty_cycle = 0.5,
            .flags = { .polarity_active_low = 0 },
        };
        rmt_apply_carrier(tx_chan_, &carrier_cfg);
        
        switch (mode) {
            case kIrJammerModeBasic: {
                uint32_t dur[] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12};
                rmt_transmit_config_t transmit_config = { .loop_count = 50, .flags = { .eot_level = 0 } };
                rmt_transmit(tx_chan_, raw_encoder_, dur, sizeof(dur), &transmit_config);
                rmt_tx_wait_all_done(tx_chan_, -1);
                break;
            }
            case kIrJammerModeSweep: {
                static int sweep_val = 15;
                static int dir = 1;
                uint32_t dur[] = {(uint32_t)sweep_val, (uint32_t)sweep_val};
                rmt_transmit_config_t transmit_config = { .loop_count = 20, .flags = { .eot_level = 0 } };
                rmt_transmit(tx_chan_, raw_encoder_, dur, sizeof(dur), &transmit_config);
                rmt_tx_wait_all_done(tx_chan_, -1);
                
                sweep_val += dir;
                if (sweep_val > 70 || sweep_val < 8) dir *= -1;
                break;
            }
            case kIrJammerModeRandom: {
                uint32_t dur[20];
                for(int i=0; i<20; i++) dur[i] = (rand() % 990) + 10;
                rmt_transmit_config_t transmit_config = { .loop_count = 0, .flags = { .eot_level = 0 } };
                rmt_transmit(tx_chan_, raw_encoder_, dur, sizeof(dur), &transmit_config);
                rmt_tx_wait_all_done(tx_chan_, -1);
                if (rand() % 10 < 3) freq_idx = rand() % 7;
                break;
            }
            default:
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
