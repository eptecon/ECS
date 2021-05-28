# put your *.o targets here, make should handle the rest!
SRCS =  $(wildcard src/*.c)
SRCS += $(wildcard libs/FreeRTOS/*.c)

SRCS += libs/FreeRTOS/portable/GCC/ARM_CM0/port.c
SRCS += libs/FreeRTOS/portable/MemMang/heap_1.c

SRCS += libs/tinyprintf/tinyprintf.c
SRCS += libs/contiki_ringbuf/ringbuf.c

SRCS += ../common/spi_protocol/spi_protocol_crc8.c

INCLUDES =  -I inc
INCLUDES += -I libs/FreeRTOS/include
INCLUDES += -I libs/FreeRTOS/portable/GCC/ARM_CM0

INCLUDES += -I libs/tinyprintf

INCLUDES += -I libs/contiki_ringbuf

INCLUDES += -I ../common/spi_protocol

BUILDDIR=build

# all the files will be generated with this name (main.elf, main.bin, main.hex, etc)
PROJ_NAME=fw_stm32_$(VER)

# Location of the Libraries folder from the STM32F0xx Standard Peripheral Library
STD_PERIPH_LIB=libs

# Location of the linker scripts
LDSCRIPT_INC=ldscripts

# that's it, no need to change anything below this line!

###################################################

CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-objdump
SIZE=arm-none-eabi-size

CFLAGS  = -Wall -g -std=c99 -Os  
CFLAGS += $(INCLUDES)
#CFLAGS += -mlittle-endian -mthumb -mcpu=cortex-m0 -march=armv6s-m
CFLAGS += -mlittle-endian -mcpu=cortex-m0  -march=armv6-m -mthumb
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wl,--gc-sections -Wl,-Map=$(BUILDDIR)/$(PROJ_NAME).map

###################################################

vpath %.c src
vpath %.a $(STD_PERIPH_LIB)

ROOT=$(shell pwd)

CFLAGS += -I inc -I $(STD_PERIPH_LIB) -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F0xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include -I $(STD_PERIPH_LIB)/STM32F0xx_StdPeriph_Driver/inc
CFLAGS += -include $(STD_PERIPH_LIB)/stm32f0xx_conf.h

SRCS += src/startup_stm32f0xx.s # add startup file to build

# need if you want to build with -DUSE_CMSIS 
#SRCS += stm32f0_discovery.c
#SRCS += stm32f0_discovery.c stm32f0xx_it.c

OBJS = $(SRCS:.c=.o)

###################################################

.PHONY: lib proj

all: lib proj

lib:
	$(MAKE) -C $(STD_PERIPH_LIB)

proj: 	$(BUILDDIR)/$(PROJ_NAME).elf

$(BUILDDIR)/$(PROJ_NAME).elf: $(SRCS)
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $^ -o $@ -L$(STD_PERIPH_LIB) -lstm32f0 -L$(LDSCRIPT_INC) -Tstm32f0.ld
	@$(OBJCOPY) -O ihex $(BUILDDIR)/$(PROJ_NAME).elf $(BUILDDIR)/$(PROJ_NAME).hex
	@$(OBJCOPY) -O binary $(BUILDDIR)/$(PROJ_NAME).elf $(BUILDDIR)/$(PROJ_NAME).bin
	@$(OBJDUMP) -St $(BUILDDIR)/$(PROJ_NAME).elf >$(BUILDDIR)/$(PROJ_NAME).lst
	$(SIZE) $(BUILDDIR)/$(PROJ_NAME).elf
	
program: $(BUILDDIR)/$(PROJ_NAME).bin
	st-flash write $< 0x08000000

clean:
	@rm -f *.o
	@rm -rf $(BUILDDIR)

distclean: clean
	$(MAKE) -C $(STD_PERIPH_LIB) clean
