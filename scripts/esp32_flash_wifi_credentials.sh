#! /usr/bin/env bash

echo "Enter SSID:"
read SSID
echo "Enter password:"
read PWD

echo \
"$SSID
$PWD

" > .credentials_tmp.txt

python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
	0xd9000 .credentials_tmp.txt

rm .credentials_tmp.txt
