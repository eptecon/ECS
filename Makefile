.PHONY: clean stm32_clean esp32_clean

VERSION=$(shell git rev-parse HEAD | head -c 6)

ESP32_BIN=output/fw_esp32_$(VERSION).bin
STM32_BIN=output/fw_stm32_$(VERSION).bin

all: esp32_fw stm32_fw
	@rm -rf output
	@mkdir -p output
	@cp stm32_src/build/fw_stm32_$(VERSION).bin output
	@cp esp32_src/build/fw_esp32_$(VERSION).bin output

clean: stm32_clean esp32_clean

$(STM32_BIN):
	make -C stm32_src VER=$(VERSION)


$(ESP32_BIN):
	make -C esp32_src esp_ota_loader esp_app VER=$(VERSION)

esp32_fw:
	@rm -f esp32_src/build/fw_esp32_*
	make -C esp32_src esp_ota_loader esp_app VER=$(VERSION)
	@rm -f output/fw_esp32_*
	@cp esp32_src/build/fw_esp32_$(VERSION).bin output

stm32_fw:
	make -C stm32_src VER=$(VERSION)
	@rm -f output/fw_stm32_*
	@cp stm32_src/build/fw_stm32_$(VERSION).bin output

stm32_clean:
	rm -f output/fw_stm32_*.bin
	make -C stm32_src distclean

esp32_clean:
	rm -f output/fw_esp32_*.bin
	make -C esp32_src clean
