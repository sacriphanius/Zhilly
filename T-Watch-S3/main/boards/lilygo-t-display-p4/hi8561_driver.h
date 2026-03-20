
#pragma once

#include <stdint.h>
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_mipi_dsi.h"

typedef struct
{
    int cmd;
    const void *data;
    size_t data_bytes;
    unsigned int delay_ms;
} hi8561_lcd_init_cmd_t;

typedef struct
{
    const hi8561_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
    struct
    {
        esp_lcd_dsi_bus_handle_t dsi_bus;
        const esp_lcd_dpi_panel_config_t *dpi_config;
        uint8_t lane_num;
    } mipi_config;
} hi8561_vendor_config_t;

esp_err_t esp_lcd_new_panel_hi8561(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

#endif