#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int cmd;                
    const void *data;       
    size_t data_bytes;      
    unsigned int delay_ms;  
} gc9d01n_lcd_init_cmd_t;

typedef struct {
    const gc9d01n_lcd_init_cmd_t *init_cmds;     
    uint16_t init_cmds_size;                    
} gc9d01n_vendor_config_t;

esp_err_t esp_lcd_new_panel_gc9d01n(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

#define GC9D01N_PANEL_BUS_SPI_CONFIG(sclk, mosi, max_trans_sz)   \
    {                                                           \
        .mosi_io_num = mosi,                                    \
        .miso_io_num = -1,                                      \
        .sclk_io_num = sclk,                                    \
        .quadwp_io_num = -1,                                    \
        .quadhd_io_num = -1,                                    \
        .data4_io_num = -1,                                     \
        .data5_io_num = -1,                                     \
        .data6_io_num = -1,                                     \
        .data7_io_num = -1,                                     \
        .max_transfer_sz = max_trans_sz,                        \
        .flags = 0,                                             \
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,               \
        .intr_flags = 0                                         \
    }

#define GC9D01N_PANEL_IO_SPI_CONFIG(cs, dc, callback, callback_ctx)  \
    {                                                               \
        .cs_gpio_num = cs,                                          \
        .dc_gpio_num = dc,                                          \
        .spi_mode = 0,                                              \
        .pclk_hz = 80 * 1000 * 1000,                                \
        .trans_queue_depth = 10,                                    \
        .on_color_trans_done = callback,                            \
        .user_ctx = callback_ctx,                                   \
        .lcd_cmd_bits = 8,                                          \
        .lcd_param_bits = 8,                                        \
        .flags = {}                                                 \
    }

#ifdef __cplusplus
}
#endif
