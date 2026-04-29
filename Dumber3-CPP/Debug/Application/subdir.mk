################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Application/Motors.cpp \
../Application/app_main.cpp \
../Application/application.cpp \
../Application/battery_version4.cpp \
../Application/commands.cpp \
../Application/leds.cpp \
../Application/messages.cpp \
../Application/panic_version4.cpp \
../Application/rtos_support.cpp \
../Application/xbee.cpp 

OBJS += \
./Application/Motors.o \
./Application/app_main.o \
./Application/application.o \
./Application/battery_version4.o \
./Application/commands.o \
./Application/leds.o \
./Application/messages.o \
./Application/panic_version4.o \
./Application/rtos_support.o \
./Application/xbee.o 

CPP_DEPS += \
./Application/Motors.d \
./Application/app_main.d \
./Application/application.d \
./Application/battery_version4.d \
./Application/commands.d \
./Application/leds.d \
./Application/messages.d \
./Application/panic_version4.d \
./Application/rtos_support.d \
./Application/xbee.d 


# Each subdirectory must supply rules for building sources it contributes
Application/%.o Application/%.su Application/%.cyclo: ../Application/%.cpp Application/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m0plus -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L071xx -c -I../Core/Inc -I../Application -I../Drivers/STM32L0xx_HAL_Driver/Inc -I../Drivers/STM32L0xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM0 -I../Drivers/CMSIS/Device/ST/STM32L0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Application

clean-Application:
	-$(RM) ./Application/Motors.cyclo ./Application/Motors.d ./Application/Motors.o ./Application/Motors.su ./Application/app_main.cyclo ./Application/app_main.d ./Application/app_main.o ./Application/app_main.su ./Application/application.cyclo ./Application/application.d ./Application/application.o ./Application/application.su ./Application/battery_version4.cyclo ./Application/battery_version4.d ./Application/battery_version4.o ./Application/battery_version4.su ./Application/commands.cyclo ./Application/commands.d ./Application/commands.o ./Application/commands.su ./Application/leds.cyclo ./Application/leds.d ./Application/leds.o ./Application/leds.su ./Application/messages.cyclo ./Application/messages.d ./Application/messages.o ./Application/messages.su ./Application/panic_version4.cyclo ./Application/panic_version4.d ./Application/panic_version4.o ./Application/panic_version4.su ./Application/rtos_support.cyclo ./Application/rtos_support.d ./Application/rtos_support.o ./Application/rtos_support.su ./Application/xbee.cyclo ./Application/xbee.d ./Application/xbee.o ./Application/xbee.su

.PHONY: clean-Application

