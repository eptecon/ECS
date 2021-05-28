#! /usr/bin/env bash

ESP_SRC_DIR=../esp32_src
IDF_PATH=../esp-idf

killall minicom
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
	0x10000 $ESP_SRC_DIR/build/esp_ota_loader.bin \
	0x8000 $ESP_SRC_DIR/build/partitions.bin
