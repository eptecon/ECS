#! /usr/bin/env bash

#A little hack that allows ESP32 rebooting without touching it.
IDF_PATH=../esp-idf

python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset chip_id
