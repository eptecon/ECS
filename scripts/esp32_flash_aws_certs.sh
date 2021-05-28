#! /usr/bin/env bash

#./flash_aws_certs <port> <root CA> <public key> <private key>
ROOT_CA=../common/certs_3c81ebbb65/root_CA.pem
PUB_KEY=../common/certs_3c81ebbb65/*certificate.pem.crt
PRV_KEY=../common/certs_3c81ebbb65/*private.pem.key
IDF_PATH=../esp-idf

killall minicom
python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $1 --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
	0xdb000 $ROOT_CA \
	0xdc000 $PUB_KEY \
	0xdd000 $PRV_KEY


