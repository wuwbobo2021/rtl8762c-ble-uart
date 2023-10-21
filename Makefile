######################################
# target
######################################
TARGET = rtl8762c-ble-uart
ENCRYPTION = 0
######################################
# building variables
######################################
# debug build?
DEBUG = 0
# optimization
#OPT = -Og
OPT = -O1
#######################################
# paths
#######################################
# Build path
SDK_DIR = ../sdk
BUILD_DIR = build
BIN_DIR = bin
######################################
# source
######################################
# C sources
C_SOURCES =  \
$(SDK_DIR)/src/mcu/rtl876x/system_rtl8762c.c \
$(SDK_DIR)/src/mcu/peripheral/rtl876x_io_dlps.c \
$(SDK_DIR)/src/mcu/peripheral/rtl876x_rcc.c \
$(SDK_DIR)/src/mcu/peripheral/rtl876x_uart.c \
./simple_ble_service.c \
./app_task.c \
./main.c \
./peripheral_app.c \
./uart.c \
# sources END
# ASM sources
ASM_SOURCES =$(SDK_DIR)/src/mcu/rtl876x/arm/startup_rtl8762c_gcc.s
#startup_rtl8762c_ARMCC.s
#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp -c
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
OD = $(PREFIX)objdump
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S
 
#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m4
# fpu
FPU = -mfpu=fpv4-sp-d16
# float-abi
#FLOAT-ABI = -mfloat-abi=hard
FLOAT-ABI = -mfloat-abi=hard
# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)
# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS =


# AS includes
AS_INCLUDES = \

# C includes
C_INCLUDES =  \
-I$(SDK_DIR)/inc/app \
-I$(SDK_DIR)/inc/bluetooth/gap \
-I$(SDK_DIR)/inc/bluetooth/gap/gap_lib \
-I$(SDK_DIR)/inc/bluetooth/profile \
-I$(SDK_DIR)/inc/os \
-I$(SDK_DIR)/inc/peripheral \
-I$(SDK_DIR)/inc/platform \
-I$(SDK_DIR)/inc/platform/cmsis \
-I$(SDK_DIR)/src/sample/ble_peripheral \
-I. \
# includes END
#C_PRE_INCLUDES

PER_INCLUDE=  \
-include app_flags.h \
#PRE_INCLUDES END

#C_PER_DEFINE

PER_DEFINE=  \
#PER_DEFINE END

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) -specs=nano.specs $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

CFLAGS += -std=c99 
# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"
# perinclude 
ifneq ($(PER_INCLUDE), )
CFLAGS +=$(PER_INCLUDE)
endif
ifneq ($(PER_DEFINE), )
CFLAGS +=$(PER_DEFINE)
endif
#######################################
# LDFLAGS
#######################################
# link script
ifeq ($(ENCRYPTION),0)
      LDSCRIPT = app.ld 
else 
      LDSCRIPT = app-ENCRYPTION.ld
endif


LIBS = -lc -lm -lnosys 
# libraries
LIBDIR = \
$(SDK_DIR)/bin/rom_symbol_gcc.axf \
$(SDK_DIR)/bin/gap_utils.a \
# lib_end

LDFLAGS = $(MCU) -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections  -specs=nano.specs

# default action: build all
.PHONY : all
all:mem_define.ld $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex 
	-mkdir bin
	$(SDK_DIR)/tool/hex2bin/Hex2Bin build/$(TARGET).hex bin/$(TARGET).bin
	$(SDK_DIR)/tool/prepend_header/prepend_header -t app_code -p bin/$(TARGET).bin -m 1 -c crc -a $(SDK_DIR)/tool/key.json
	$(SDK_DIR)/tool/md5/md5 bin/$(TARGET)_MP.bin
	$(OD) -D -S build/$(TARGET).elf > bin/$(TARGET).dis
#	$(CC) -o all $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
mem_define.ld :
	$(SDK_DIR)/tool/memory_icf/MemDefine ./ gcc

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
#vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.c  $(dir $(C_SOURCES))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	mkdir $@		

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)
	-rm -fR $(BIN_DIR)
	-rm mem_define.ld
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
