################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
D:/gitrepo/Vi-Qi-FSK-Decode/WS_SPI_CM4/v_qi_i2c/Middlewares/ST/netxduo/common/drivers/ethernet/nx_stm32_eth_driver.c 

OBJS += \
./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.o 

C_DEPS += \
./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.d 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.o: D:/gitrepo/Vi-Qi-FSK-Decode/WS_SPI_CM4/v_qi_i2c/Middlewares/ST/netxduo/common/drivers/ethernet/nx_stm32_eth_driver.c Middlewares/Interfaces/Network/Ethernet\ Interface/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -D__CCRX__ -DCORE_CM7 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -DTX_INCLUDE_USER_DEFINE_FILE -DNX_SECURE_INCLUDE_USER_DEFINE_FILE -DNX_INCLUDE_USER_DEFINE_FILE -c -I../Core/Inc -I../I-CUBE-Mongoose/App -I"../Middlewares/I-CUBE-Mongoose/Network Library/Mongoose" -I../AZURE_RTOS/App -I../NetXDuo/App -I../NetXDuo/Target -I../../Drivers/BSP/Components/lan8742/ -I"../../Middlewares/Third_Party/Cesanta_Network Library/Mongoose" -I../../Middlewares/ST/netxduo/common/drivers/ethernet/ -I../../Middlewares/ST/netxduo/addons/dhcp/ -I../../Middlewares/ST/netxduo/common/inc/ -I../../Middlewares/ST/netxduo/ports/cortex_m7/gnu/inc/ -I../../Middlewares/ST/netxduo/nx_secure/inc/ -I../../Middlewares/ST/netxduo/nx_secure/ports/ -I../../Middlewares/ST/netxduo/crypto_libraries/inc/ -I../../Middlewares/ST/netxduo/crypto_libraries/ports/cortex_m7/gnu/inc/ -I../../Middlewares/ST/threadx/common/inc/ -I../../Middlewares/ST/threadx/ports/cortex_m7/gnu/inc/ -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"Middlewares/Interfaces/Network/Ethernet Interface/nx_stm32_eth_driver.d" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Interfaces-2f-Network-2f-Ethernet-20-Interface

clean-Middlewares-2f-Interfaces-2f-Network-2f-Ethernet-20-Interface:
	-$(RM) ./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.cyclo ./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.d ./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.o ./Middlewares/Interfaces/Network/Ethernet\ Interface/nx_stm32_eth_driver.su

.PHONY: clean-Middlewares-2f-Interfaces-2f-Network-2f-Ethernet-20-Interface

