

 #pragma once

 #include <stdint.h>
 #include "soc/soc_caps.h"

 #if SOC_MIPI_DSI_SUPPORTED
 #include "esp_lcd_panel_vendor.h"
 #include "esp_lcd_mipi_dsi.h"

 #ifdef __cplusplus
 extern "C" {
 #endif

 typedef struct {
     int cmd;                
     const void *data;       
     size_t data_bytes;      
     unsigned int delay_ms;  
 } st7123_lcd_init_cmd_t;

 typedef struct {
     const st7123_lcd_init_cmd_t *init_cmds;       
     uint16_t init_cmds_size;                        
     struct {
         esp_lcd_dsi_bus_handle_t dsi_bus;               
         const esp_lcd_dpi_panel_config_t *dpi_config;   
         uint8_t  lane_num;                              
     } mipi_config;
 } st7123_vendor_config_t;

 esp_err_t esp_lcd_new_panel_st7123(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                      esp_lcd_panel_handle_t *ret_panel);

 #define ST7123_PANEL_BUS_DSI_2CH_CONFIG()              \
     {                                                    \
         .bus_id = 0,                                     \
         .num_data_lanes = 2,                             \
         .lane_bit_rate_mbps = 1000,                      \
     }

 #define ST7123_PANEL_IO_DBI_CONFIG()  \
     {                                   \
         .virtual_channel = 0,           \
         .lcd_cmd_bits = 8,              \
         .lcd_param_bits = 8,            \
     }

 #define ST7123_800_1280_PANEL_60HZ_DPI_CONFIG(px_format) \
     {                                                      \
         .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,       \
         .dpi_clock_freq_mhz = 80,                          \
         .virtual_channel = 0,                              \
         .pixel_format = px_format,                         \
         .num_fbs = 1,                                      \
         .video_timing = {                                  \
             .h_size = 800,                                 \
             .v_size = 1280,                                \
             .hsync_back_porch = 140,                       \
             .hsync_pulse_width = 40,                       \
             .hsync_front_porch = 40,                       \
             .vsync_back_porch = 16,                        \
             .vsync_pulse_width = 4,                        \
             .vsync_front_porch = 16,                       \
         },                                                 \
         .flags.use_dma2d = true,                           \
     }
 #endif

 #ifdef __cplusplus
 }
 #endif
 