###############################################################################
# PROJECT SETTINGS
###############################################################################
TARGET = stm32_executable
BUILD_DIR = build

###############################################################################
# TOOLCHAIN DEFINITIONS
###############################################################################
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

###############################################################################
# TARGET HARDWARE ARCHITECTURE FLAGS (e.g., STM32F4 Cortex-M4)
###############################################################################
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

###############################################################################
# PROJECT FILES & DIRECTORIES
###############################################################################
# Linker Script
LDSCRIPT = STM32L432KCUX_FLASH.ld

# C Sources
C_SOURCES = \
support_functions.c \
waveshare/ssd1331.c \
waveshare/fonts.c \
main.c \
	
# Assembly Sources (Startup file)
ASM_SOURCES = \
startup_stm32l432kcux.s

# Include Directories (with -I prefix)
C_INCLUDES = \
-I. \
-ICMSIS/Include \
-Iwaveshare \
-ICMSIS/Device/ST/STM32L4xx/Include

###############################################################################
# COMPILER AND LINKER FLAGS
###############################################################################
# Macros/Defines

# Compile Flags
CFLAGS = $(MCU) $(C_INCLUDES) -O0 -Wall -fdata-sections -ffunction-sections -g -gdwarf-2

# Link Flags
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--gc-sections

###############################################################################
# BUILD RULES
###############################################################################
# Default goal
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

# List of object files
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

# Compile C files
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

# Assemble ASM files
$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

# Link files into ELF
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

# Generate HEX file
$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(CP) -O ihex $< $@

# Generate BIN file
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(CP) -O binary -S $< $@

# Create Build Directory
$(BUILD_DIR):
	mkdir $@

###############################################################################
# CLEAN & UTILITIES
###############################################################################
clean:
	rm -rf $(BUILD_DIR)

# Optional flash target using OpenOCD
flash: all
	openocd -f interface/stlink.cfg -f target/stm32l4x.cfg -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

.PHONY: all clean flash

