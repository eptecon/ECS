#!/usr/bin/env bash

pushd $IDF_PATH
git submodule init
git submodule update --recursive
git pull origin
git submodule update
popd
