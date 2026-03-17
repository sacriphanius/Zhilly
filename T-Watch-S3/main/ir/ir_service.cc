#include "ir_service.h"
#include "WORLD_IR_CODES.h"
#include <esp_log.h>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

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
            .allow_pd = 0,
        },
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &tx_chan_));
    
    rmt_carrier_config_t carrier_cfg = {
        .frequency_hz = 38000,
        .duty_cycle = 0.33,
        .flags = {
            .polarity_active_low = 0,
            .always_on = 0,
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
    stop_requested_ = false;
    running_ = true;
    TaskParams* params = new TaskParams{this, region};
    xTaskCreate(TaskWrapper, "tvbgone_task", 4096, params, 5, &task_handle_);
}

void IrService::StartJammer(IrJammerMode mode) {
    Stop();
    stop_requested_ = false;
    running_ = true;
    TaskParams* params = new TaskParams{this, std::to_string((int)mode)};
    xTaskCreate(TaskWrapper, "jammer_task", 4096, params, 5, &task_handle_);
}

void IrService::Stop() {
    stop_requested_ = true;
    if (task_handle_ != nullptr) {
        int wait_ms = 0;
        while (running_ && wait_ms < 500) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_ms += 10;
        }
        if (running_) { // Force delete if still running
            vTaskDelete(task_handle_);
        }
        task_handle_ = nullptr;
        running_ = false;
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

    for (int i = 0; i < max_idx && !stop_requested_; ++i) {
        const IrCode* code = codes[i];
        if (code == nullptr) break;
        
        uint32_t freq = code->timer_val * 1000;
        rmt_carrier_config_t carrier_cfg = {
            .frequency_hz = freq,
            .duty_cycle = 0.33,
            .flags = { 
                .polarity_active_low = 0,
                .always_on = 0 
            },
        };
        rmt_apply_carrier(tx_chan_, &carrier_cfg);
        
        std::vector<rmt_symbol_word_t> symbols;
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
            
            rmt_symbol_word_t symbol;
            symbol.val = 0;
            symbol.val =  (code->times[index * 2] * 10) | (1 << 15); // ON
            symbol.val |= (code->times[index * 2 + 1] * 10) << 16;   // OFF
            symbols.push_back(symbol);
        }
        
        rmt_transmit_config_t transmit_config = { .loop_count = 0, .flags = { .eot_level = 0 } };
        rmt_transmit(tx_chan_, raw_encoder_, symbols.data(), symbols.size() * sizeof(rmt_symbol_word_t), &transmit_config);
        rmt_tx_wait_all_done(tx_chan_, -1);
        
        vTaskDelay(pdMS_TO_TICKS(205));
    }
}

void IrService::RunJammer(IrJammerMode mode) {
    uint32_t freqs[] = {30000, 33000, 36000, 38000, 40000, 42000, 56000};
    int freq_idx = 3; // 38kHz default
    rmt_transmit_config_t tx_cfg = { .loop_count = 0, .flags = { .eot_level = 0 } };

    while (!stop_requested_) {
        uint32_t freq = freqs[freq_idx];
        rmt_carrier_config_t carrier_cfg = {};
        carrier_cfg.frequency_hz = freq;
        carrier_cfg.duty_cycle = 0.33f;
        carrier_cfg.flags.polarity_active_low = 0;
        carrier_cfg.flags.always_on = 0;
        rmt_apply_carrier(tx_chan_, &carrier_cfg);

        switch (mode) {
            case kIrJammerModeBasic: {
                // 500us ON / 500us OFF — enough for ~19 carrier cycles at 38kHz
                rmt_symbol_word_t sym;
                sym.level0    = 1;
                sym.duration0 = 500;
                sym.level1    = 0;
                sym.duration1 = 500;
                for (int r = 0; r < 20 && !stop_requested_; ++r) {
                    rmt_transmit(tx_chan_, raw_encoder_, &sym, sizeof(sym), &tx_cfg);
                    rmt_tx_wait_all_done(tx_chan_, 100);
                }
                vTaskDelay(pdMS_TO_TICKS(5));
                break;
            }
            case kIrJammerModeEnhanced: {
                // Asymmetric: 600us ON / 200us OFF — more aggressive
                rmt_symbol_word_t sym;
                sym.level0    = 1;
                sym.duration0 = 600;
                sym.level1    = 0;
                sym.duration1 = 200;
                for (int r = 0; r < 20 && !stop_requested_; ++r) {
                    rmt_transmit(tx_chan_, raw_encoder_, &sym, sizeof(sym), &tx_cfg);
                    rmt_tx_wait_all_done(tx_chan_, 100);
                }
                vTaskDelay(pdMS_TO_TICKS(5));
                break;
            }
            case kIrJammerModeSweep: {
                // Sweep mark duration 200us..1200us
                static int sweep_val = 200;
                static int dir = 10;
                rmt_symbol_word_t sym;
                sym.level0    = 1;
                sym.duration0 = (uint16_t)sweep_val;
                sym.level1    = 0;
                sym.duration1 = 300;
                for (int r = 0; r < 10 && !stop_requested_; ++r) {
                    rmt_transmit(tx_chan_, raw_encoder_, &sym, sizeof(sym), &tx_cfg);
                    rmt_tx_wait_all_done(tx_chan_, 100);
                }
                sweep_val += dir;
                if (sweep_val > 1200 || sweep_val < 200) dir *= -1;
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
            }
            case kIrJammerModeRandom: {
                // Random burst length 100-2000us, random frequency
                rmt_symbol_word_t sym;
                uint16_t dur = (uint16_t)((rand() % 1900) + 100);
                sym.level0    = 1;
                sym.duration0 = dur;
                sym.level1    = 0;
                sym.duration1 = (uint16_t)((rand() % 900) + 100);
                rmt_transmit(tx_chan_, raw_encoder_, &sym, sizeof(sym), &tx_cfg);
                rmt_tx_wait_all_done(tx_chan_, 200);
                if (rand() % 10 < 3) freq_idx = rand() % 7;
                vTaskDelay(pdMS_TO_TICKS(20));
                break;
            }
            case kIrJammerModeEmpty: {
                // Very short burst + long silence to confuse AGC in receivers
                rmt_symbol_word_t sym;
                sym.level0    = 1;
                sym.duration0 = 100;
                sym.level1    = 0;
                sym.duration1 = 5000; // 5ms silence (max 15-bit = 32767)
                rmt_transmit(tx_chan_, raw_encoder_, &sym, sizeof(sym), &tx_cfg);
                rmt_tx_wait_all_done(tx_chan_, 200);
                if (rand() % 5 < 2) freq_idx = (freq_idx + 1) % 7;
                vTaskDelay(pdMS_TO_TICKS(50));
                break;
            }
            default:
                vTaskDelay(pdMS_TO_TICKS(10));
                break;
        }
    }
}

