#!/usr/bin/env bash

ROOT_CA=../common/certs_3c81ebbb65/root_CA.pem
PUB_KEY=../common/certs_3c81ebbb65/*certificate.pem.crt
PRV_KEY=../common/certs_3c81ebbb65/*private.pem.key

mosquitto_sub \
	--cert $PUB_KEY \
	--key $PRV_KEY \
	--cafile $ROOT_CA \
	-h a3n07wc0hcy897.iot.eu-central-1.amazonaws.com \
	-p 8883 \
	-t $1 -v
