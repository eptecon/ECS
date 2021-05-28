#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

IDF_PATH=../esp-idf
EXTRA_COMPONENT_DIRS += esp_ota
PROJECT_NAME=esp_ota_loader
include $(IDF_PATH)/make/project.mk
