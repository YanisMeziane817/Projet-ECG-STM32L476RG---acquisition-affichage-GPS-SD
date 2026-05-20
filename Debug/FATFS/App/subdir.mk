################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FATFS/App/fatfs.c 

C_DEPS += \
./FATFS/App/fatfs.d 

OBJS += \
./FATFS/App/fatfs.o 


# Each subdirectory must supply rules for building sources it contributes
FATFS/App/%.o FATFS/App/%.su FATFS/App/%.cyclo: ../FATFS/App/%.c FATFS/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Core/Inc -IC:/Users/yanis/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Drivers/STM32L4xx_HAL_Driver/Inc -IC:/Users/yanis/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -IC:/Users/yanis/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Drivers/CMSIS/Device/ST/STM32L4xx/Include -IC:/Users/yanis/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Drivers/CMSIS/Include -I../FATFS/Target -I../FATFS/App -IC:/Users/yanis/STM32Cube/Repository/STM32Cube_FW_L4_V1.18.2/Middlewares/Third_Party/FatFs/src -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FATFS-2f-App

clean-FATFS-2f-App:
	-$(RM) ./FATFS/App/fatfs.cyclo ./FATFS/App/fatfs.d ./FATFS/App/fatfs.o ./FATFS/App/fatfs.su

.PHONY: clean-FATFS-2f-App

