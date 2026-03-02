

#pragma once

#include <stdint.h>

#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_rgb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int cmd;                
    const void *data;       
    size_t data_bytes;      
    unsigned int delay_ms;  
} gc9503_lcd_init_cmd_t;

typedef struct {
    const esp_lcd_rgb_panel_config_t *rgb_config;   
    const gc9503_lcd_init_cmd_t *init_cmds;         
    uint16_t init_cmds_size;                        
    struct {
        unsigned int mirror_by_cmd: 1;              
        unsigned int auto_del_panel_io: 1;          
    } flags;
} gc9503_vendor_config_t;

esp_err_t esp_lcd_new_panel_gc9503(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#define GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_cfg, scl_active_edge) \
    {                                                               \
        .line_config = line_cfg,                                    \
        .expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX,             \
        .spi_mode = scl_active_edge ? 1 : 0,                        \
        .lcd_cmd_bytes = 1,                                         \
        .lcd_param_bytes = 1,                                       \
        .flags = {                                                  \
            .use_dc_bit = 1,                                        \
            .dc_zero_on_data = 0,                                   \
            .lsb_first = 0,                                         \
            .cs_high_active = 0,                                    \
            .del_keep_cs_inactive = 1,                              \
        },                                                          \
    }

#define GC9503_376_960_PANEL_60HZ_RGB_TIMING()      \
    {                                               \
        .pclk_hz = 16 * 1000 * 1000,                \
        .h_res = 376,                               \
        .v_res = 960,                               \
        .hsync_pulse_width = 8,                     \
        .hsync_back_porch = 30,                     \
        .hsync_front_porch = 30,                    \
        .vsync_pulse_width = 8,                     \
        .vsync_back_porch = 16,                     \
        .vsync_front_porch = 16,                    \
        .flags = {                                  \
            .hsync_idle_low = 0,                    \
            .vsync_idle_low = 0,                    \
            .de_idle_high = 0,                      \
            .pclk_active_neg = 0,                   \
            .pclk_idle_high = 0,                    \
        },                                          \
    }

#ifdef __cplusplus
}
#endif
