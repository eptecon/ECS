#! /usr/bin/env bash

IDF_PATH=../esp-idf

killall minicom
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 erase_flash
