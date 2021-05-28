#! /usr/bin/env bash

ESP_SRC_DIR=../esp32_src/
SETTINGS=$2
IDF_PATH=../esp-idf

killall minicom
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
	0xde000 $SETTINGS
