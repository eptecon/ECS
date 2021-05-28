#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

esp_app: bootloader
	@echo "$(VER)" > version.txt
	@echo	"#ifndef VERSION_H\n"\
		"#define VERSION_H\n"\
		"#define FW_VERSION \"$(VER)\"\n"\
		"#endif /* VERSION_H */" > esp_app/fw_version.h
	@make -s -f app.mk 

esp_ota_loader: bootloader
	@make -f ota.mk PROJECT_NAME=esp_ota_loader

%:
	@make -f app.mk $@
print-%:
	@echo $* = $($*)
