#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

IDF_PATH=../esp-idf
EXTRA_COMPONENT_DIRS += esp_app
COMPONENT_EXTRA_INCLUDES += ../common/spi_protocol
COMPONENT_ADD_INCLUDEDIRS += ../common/spi_protocol
PROJECT_NAME=fw_esp32_$(VER)

include $(IDF_PATH)/make/project.mk
