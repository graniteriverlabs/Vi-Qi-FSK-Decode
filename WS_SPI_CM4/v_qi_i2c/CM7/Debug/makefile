################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include NetXDuo/App/subdir.mk
-include Middlewares/ThreadX/RTOS/ThreadX/Core/subdir.mk
-include Middlewares/NetXDuo/Network/NetXDuo/TLS/subdir.mk
-include Middlewares/NetXDuo/Network/NetXDuo/NX\ Core/subdir.mk
-include Middlewares/NetXDuo/Network/NetXDuo/Crypto/subdir.mk
-include Middlewares/NetXDuo/Network/NetXDuo/Addons\ DHCP\ Client/subdir.mk
-include Middlewares/Interfaces/Network/Ethernet\ Phy\ Interface/LAN8742_Phy_Interface/subdir.mk
-include Middlewares/Interfaces/Network/Ethernet\ Interface/subdir.mk
-include Middlewares/I-CUBE-Mongoose/Network\ Library/Mongoose/subdir.mk
-include Drivers/STM32H7xx_HAL_Driver/subdir.mk
-include Drivers/BSP/STM32H7xx_Nucleo/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Common/Src/subdir.mk
-include AZURE_RTOS/App/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := v_qi_i2c_CM7
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
v_qi_i2c_CM7.elf \

MAP_FILES += \
v_qi_i2c_CM7.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
v_qi_i2c_CM7.list \

OBJCOPY_BIN += \
v_qi_i2c_CM7.bin \


# All Target
all:
	+@$(MAKE) --no-print-directory main-build && $(MAKE) --no-print-directory post-build

# Main-build Target
main-build: v_qi_i2c_CM7.elf secondary-outputs

# Tool invocations
v_qi_i2c_CM7.elf v_qi_i2c_CM7.map: $(OBJS) $(USER_OBJS) D:\gitrepo\Vi-Qi-FSK-Decode\WS_SPI_CM4\v_qi_i2c\CM7\STM32H755ZITX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "v_qi_i2c_CM7.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m7 -T"D:\gitrepo\Vi-Qi-FSK-Decode\WS_SPI_CM4\v_qi_i2c\CM7\STM32H755ZITX_FLASH.ld" --specs=nosys.specs -Wl,-Map="v_qi_i2c_CM7.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

v_qi_i2c_CM7.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "v_qi_i2c_CM7.list"
	@echo 'Finished building: $@'
	@echo ' '

v_qi_i2c_CM7.bin: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objcopy  -O binary $(EXECUTABLES) "v_qi_i2c_CM7.bin"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) default.size.stdout v_qi_i2c_CM7.bin v_qi_i2c_CM7.elf v_qi_i2c_CM7.list v_qi_i2c_CM7.map
	-@echo ' '

post-build:
	mkdir -p ../../out
	arm-none-eabi-objcopy -O binary "v_qi_i2c_CM7.elf" "../../out/cm7.bin"
	arm-none-eabi-objcopy -O binary "../../CM4/Debug/v_qi_i2c_CM4.elf" "../../out/cm4.bin"
	python "../../merge_ota_image.py" -o "../../out/ota_image.bin" --max-size 0x100000 --seg "../../out/cm7.bin" 0x0 --seg "../../out/cm4.bin" 0xA0000
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(OBJCOPY_BIN)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified post-build

-include ../makefile.targets
