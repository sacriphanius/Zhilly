

To flash all binaries with a single command, open a terminal in this folder and run:

esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x20000 xiaozhi.bin 0x800000 expression_assets.bin

(Note: Change /dev/ttyACM0 to your actual port if different)