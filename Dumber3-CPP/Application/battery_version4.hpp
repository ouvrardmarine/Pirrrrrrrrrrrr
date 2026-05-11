#ifndef INC_BATTERY_HPP_
#define INC_BATTERY_HPP_

#ifdef __cplusplus
extern "C" {
#endif

#include "application.h"
#include "messages.h"
#include "stm32l0xx.h"

class Battery {
public:
    static void Init();

    ///////////////////////////////////////////////////////////////////////////////
    static void TimerCallback(TimerHandle_t xTimer);
    static void AdcConvCpltCallback(ADC_HandleTypeDef* hadc);
    static void GpioExtiCallback(uint16_t GPIO_Pin);
    ///////////////////////////////////////////////////////////////////////////////

    static void suspend(void);

private:
    // Charger state
    enum class ChargerStatus {
        NotPlugged,
        Charging,
        ChargeComplete,
        Error
    };

    // Task
    static void Thread(void* params);

    // Core functions
    static ChargerStatus GetChargerStatus();
    static int GetVoltage(uint16_t* val);
    static uint16_t BatteryLevel(uint8_t voltage, ChargerStatus status);

    // Callbacks

    // Static members
    static TaskHandle_t taskHandle;
    static TaskHandle_t adcWaitingTask;

    static uint16_t adcRawValue;
    static uint8_t buttonInactivity;

    // RTOS objects
    static StaticTask_t taskBuffer;
    static StackType_t stack[STACK_SIZE];

    static StaticTimer_t timerBuffer;
    static TimerHandle_t timerHandle;
};

#ifdef __cplusplus
}
#endif

#endif /* INC_BATTERY_HPP_ */
