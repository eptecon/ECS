IDF_PATH=../esp-idf

python $IDF_PATH/components/esptool_py/esptool/esptool.py -p $1 read_mac
