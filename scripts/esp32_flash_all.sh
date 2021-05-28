#! /usr/bin/env bash

ESP_SRC_DIR=../esp32_src

IDF_PATH=../esp-idf


ROOT_CA=../common/certs_3c81ebbb65/root_CA.pem
PUB_KEY=../common/certs_3c81ebbb65/*certificate.pem.crt
PRV_KEY=../common/certs_3c81ebbb65/*private.pem.key

echo "flashing firmware version `cat $ESP_SRC_DIR/version.txt`"

killall minicom
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
	0x1000 ../esp32_src/build/bootloader/bootloader.bin \
	0x10000 ../esp32_src/build/esp_ota_loader.bin \
	0xe0000 ../esp32_src/build/fw_esp32_`cat $ESP_SRC_DIR/version.txt`.bin \
	0x8000 ../esp32_src/build/partitions.bin \
	0xdb000 $ROOT_CA \
	0xdc000 $PUB_KEY \
	0xdd000 $PRV_KEY \
	0xde000 settings.json
