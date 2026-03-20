T-Watch-S3 Firmware V1.1.2 - Flashing Instructions

To flash this firmware to your T-Watch-S3, use the following esptool command from this directory:

python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m 0x0 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x20000 xiaozhi.bin 0x800000 expression_assets.bin