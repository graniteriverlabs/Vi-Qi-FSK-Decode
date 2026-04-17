################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/cm4_grl_app.c \
../Core/Src/dma.c \
../Core/Src/eth.c \
../Core/Src/fsk_decoder.c \
../Core/Src/fsk_online_stream.c \
../Core/Src/fsk_stream_test.c \
../Core/Src/gpio.c \
../Core/Src/hrtim.c \
../Core/Src/ipc_shared.c \
../Core/Src/ipc_stream_shared.c \
../Core/Src/main.c \
../Core/Src/qi.c \
../Core/Src/spi.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/tim.c 

OBJS += \
./Core/Src/cm4_grl_app.o \
./Core/Src/dma.o \
./Core/Src/eth.o \
./Core/Src/fsk_decoder.o \
./Core/Src/fsk_online_stream.o \
./Core/Src/fsk_stream_test.o \
./Core/Src/gpio.o \
./Core/Src/hrtim.o \
./Core/Src/ipc_shared.o \
./Core/Src/ipc_stream_shared.o \
./Core/Src/main.o \
./Core/Src/qi.o \
./Core/Src/spi.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/tim.o 

C_DEPS += \
./Core/Src/cm4_grl_app.d \
./Core/Src/dma.d \
./Core/Src/eth.d \
./Core/Src/fsk_decoder.d \
./Core/Src/fsk_online_stream.d \
./Core/Src/fsk_stream_test.d \
./Core/Src/gpio.d \
./Core/Src/hrtim.d \
./Core/Src/ipc_shared.d \
./Core/Src/ipc_stream_shared.d \
./Core/Src/main.d \
./Core/Src/qi.d \
./Core/Src/spi.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/tim.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/fsk_decoder.o: ../Core/Src/fsk_decoder.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/fsk_online_stream.o: ../Core/Src/fsk_online_stream.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/fsk_stream_test.o: ../Core/Src/fsk_stream_test.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Core/Src/ipc_stream_shared.o: ../Core/Src/ipc_stream_shared.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DCORE_CM4 -DUSE_HAL_DRIVER -DSTM32H755xx -DUSE_PWR_DIRECT_SMPS_SUPPLY -DUSE_NUCLEO_64 -c -I../Core/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/BSP/STM32H7xx_Nucleo -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/cm4_grl_app.cyclo ./Core/Src/cm4_grl_app.d ./Core/Src/cm4_grl_app.o ./Core/Src/cm4_grl_app.su ./Core/Src/dma.cyclo ./Core/Src/dma.d ./Core/Src/dma.o ./Core/Src/dma.su ./Core/Src/eth.cyclo ./Core/Src/eth.d ./Core/Src/eth.o ./Core/Src/eth.su ./Core/Src/fsk_decoder.cyclo ./Core/Src/fsk_decoder.d ./Core/Src/fsk_decoder.o ./Core/Src/fsk_decoder.su ./Core/Src/fsk_online_stream.cyclo ./Core/Src/fsk_online_stream.d ./Core/Src/fsk_online_stream.o ./Core/Src/fsk_online_stream.su ./Core/Src/fsk_stream_test.cyclo ./Core/Src/fsk_stream_test.d ./Core/Src/fsk_stream_test.o ./Core/Src/fsk_stream_test.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/hrtim.cyclo ./Core/Src/hrtim.d ./Core/Src/hrtim.o ./Core/Src/hrtim.su ./Core/Src/ipc_shared.cyclo ./Core/Src/ipc_shared.d ./Core/Src/ipc_shared.o ./Core/Src/ipc_shared.su ./Core/Src/ipc_stream_shared.cyclo ./Core/Src/ipc_stream_shared.d ./Core/Src/ipc_stream_shared.o ./Core/Src/ipc_stream_shared.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/qi.cyclo ./Core/Src/qi.d ./Core/Src/qi.o ./Core/Src/qi.su ./Core/Src/spi.cyclo ./Core/Src/spi.d ./Core/Src/spi.o ./Core/Src/spi.su ./Core/Src/stm32h7xx_hal_msp.cyclo ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_hal_msp.su ./Core/Src/stm32h7xx_it.cyclo ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/stm32h7xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su

.PHONY: clean-Core-2f-Src

