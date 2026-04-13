################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/app_threadx.c \
../Core/Src/cm7_qi_app.c \
../Core/Src/dma.c \
../Core/Src/eth.c \
../Core/Src/fault.c \
../Core/Src/gpio.c \
../Core/Src/i2c.c \
../Core/Src/ipc_shared.c \
../Core/Src/lan8742.c \
../Core/Src/main.c \
../Core/Src/nxd_bsd.c \
../Core/Src/qi_peripheral.c \
../Core/Src/qi_tim.c \
../Core/Src/rng.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_hal_timebase_tim.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/usart.c 

S_UPPER_SRCS += \
../Core/Src/tx_initialize_low_level.S 

OBJS += \
./Core/Src/app_threadx.o \
./Core/Src/cm7_qi_app.o \
./Core/Src/dma.o \
./Core/Src/eth.o \
./Core/Src/fault.o \
./Core/Src/gpio.o \
./Core/Src/i2c.o \
./Core/Src/ipc_shared.o \
./Core/Src/lan8742.o \
./Core/Src/main.o \
./Core/Src/nxd_bsd.o \
./Core/Src/qi_peripheral.o \
./Core/Src/qi_tim.o \
./Core/Src/rng.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_hal_timebase_tim.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/tx_initialize_low_level.o \
./Core/Src/usart.o 

S_UPPER_DEPS += \
./Core/Src/tx_initialize_low_level.d 

C_DEPS += \
./Core/Src/app_threadx.d \
./Core/Src/cm7_qi_app.d \
./Core/Src/dma.d \
./Core/Src/eth.d \
./Core/Src/fault.d \
./Core/Src/gpio.d \
./Core/Src/i2c.d \
./Core/Src/ipc_shared.d \
./Core/Src/lan8742.d \
./Core/Src/main.d \
./Core/Src/nxd_bsd.d \
./Core/Src/qi_peripheral.d \
./Core/Src/qi_tim.d \
./Core/Src/rng.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_hal_timebase_tim.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/usart.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -D__CCRX__ -DCORE_CM7 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -DTX_INCLUDE_USER_DEFINE_FILE -DNX_SECURE_INCLUDE_USER_DEFINE_FILE -DNX_INCLUDE_USER_DEFINE_FILE -c -I../Core/Inc -I../I-CUBE-Mongoose/App -I"../Middlewares/I-CUBE-Mongoose/Network Library/Mongoose" -I../AZURE_RTOS/App -I../NetXDuo/App -I../NetXDuo/Target -I../../Drivers/BSP/Components/lan8742/ -I"../../Middlewares/Third_Party/Cesanta_Network Library/Mongoose" -I../../Middlewares/ST/netxduo/common/drivers/ethernet/ -I../../Middlewares/ST/netxduo/addons/dhcp/ -I../../Middlewares/ST/netxduo/common/inc/ -I../../Middlewares/ST/netxduo/ports/cortex_m7/gnu/inc/ -I../../Middlewares/ST/netxduo/nx_secure/inc/ -I../../Middlewares/ST/netxduo/nx_secure/ports/ -I../../Middlewares/ST/netxduo/crypto_libraries/inc/ -I../../Middlewares/ST/netxduo/crypto_libraries/ports/cortex_m7/gnu/inc/ -I../../Middlewares/ST/threadx/common/inc/ -I../../Middlewares/ST/threadx/ports/cortex_m7/gnu/inc/ -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/%.o: ../Core/Src/%.S Core/Src/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m7 -g3 -DDEBUG -DTX_INCLUDE_USER_DEFINE_FILE -c -I../Core/Inc -I"../Middlewares/I-CUBE-Mongoose/Network Library/Mongoose" -I../I-CUBE-Mongoose/App -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/app_threadx.cyclo ./Core/Src/app_threadx.d ./Core/Src/app_threadx.o ./Core/Src/app_threadx.su ./Core/Src/cm7_qi_app.cyclo ./Core/Src/cm7_qi_app.d ./Core/Src/cm7_qi_app.o ./Core/Src/cm7_qi_app.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/eth.cyclo ./Core/Src/eth.d ./Core/Src/eth.o ./Core/Src/eth.su ./Core/Src/fault.cyclo ./Core/Src/fault.d ./Core/Src/fault.o ./Core/Src/fault.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/i2c.cyclo ./Core/Src/i2c.d ./Core/Src/i2c.o ./Core/Src/i2c.su ./Core/Src/ipc_shared.cyclo ./Core/Src/ipc_shared.d ./Core/Src/ipc_shared.o ./Core/Src/ipc_shared.su ./Core/Src/lan8742.cyclo ./Core/Src/lan8742.d ./Core/Src/lan8742.o ./Core/Src/lan8742.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/nxd_bsd.cyclo ./Core/Src/nxd_bsd.d ./Core/Src/nxd_bsd.o ./Core/Src/nxd_bsd.su ./Core/Src/qi_peripheral.cyclo ./Core/Src/qi_peripheral.d ./Core/Src/qi_peripheral.o ./Core/Src/qi_peripheral.su ./Core/Src/qi_tim.cyclo ./Core/Src/qi_tim.d ./Core/Src/qi_tim.o ./Core/Src/qi_tim.su ./Core/Src/rng.cyclo ./Core/Src/rng.d ./Core/Src/rng.o ./Core/Src/rng.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_hal_timebase_tim.cyclo ./Core/Src/stm32h7xx_hal_timebase_tim.d ./Core/Src/stm32h7xx_hal_timebase_tim.o ./Core/Src/stm32h7xx_hal_timebase_tim.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/tx_initialize_low_level.d ./Core/Src/tx_initialize_low_level.o ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su

.PHONY: clean-Core-2f-Src

