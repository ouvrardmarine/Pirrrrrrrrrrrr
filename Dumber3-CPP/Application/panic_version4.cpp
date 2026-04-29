#include "panic.hpp"

extern "C" {
#include "application.h"
#include "timers.h"
#include "leds.h"
#include "stm32fxxx_hal.h"   // adapt to your MCU
}

// Forward declarations (C linkage if needed)
extern "C" void MOTORS_PowerOff(void);

// FreeRTOS handles
extern TaskHandle_t xHandleLedsHandler;
extern TaskHandle_t xHandleLedsAction;
extern TaskHandle_t xHandleBattery;
extern TimerHandle_t xHandleTimerButton;
extern TaskHandle_t xHandleApplicationMain;
extern TimerHandle_t xHandleTimerTimeout;
extern TaskHandle_t xHandleMotors;
extern TaskHandle_t xHandleMotorsControl;
extern TaskHandle_t xHandleXbeeTXHandler;
extern TaskHandle_t xHandleXbeeRX;

// Private function
[[noreturn]] static void Panic_StopTasksAndWait(void)
{
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();

    // Stop tasks (except current + LEDs)
    if (currentTask != xHandleXbeeRX)
        vTaskSuspend(xHandleXbeeRX);

    if (currentTask != xHandleXbeeTXHandler)
        vTaskSuspend(xHandleXbeeTXHandler);

    if (currentTask != xHandleMotors)
        vTaskSuspend(xHandleMotors);

    if (currentTask != xHandleMotorsControl)
        vTaskSuspend(xHandleMotorsControl);

    if (currentTask != xHandleBattery)
        vTaskSuspend(xHandleBattery);

    if (currentTask != xHandleApplicationMain)
        vTaskSuspend(xHandleApplicationMain);

    // Power off motors
    MOTORS_PowerOff();

    // Disable XBEE
    HAL_GPIO_WritePin(XBEE_RESET_GPIO_Port, XBEE_RESET_Pin, GPIO_PIN_RESET);

    // Suspend current task
    vTaskSuspend(currentTask);

    // Infinite low-power wait
    while (true) {
        __WFE();
    }
}

[[noreturn]] void Panic_Raise(PanicType panicId)
{
    switch (panicId) {
        case PanicType::AdcError:
            LEDS_Set(leds_error_1);
            break;

        case PanicType::ChargerError:
            LEDS_Set(leds_error_2);
            break;

        case PanicType::MallocError:
            LEDS_Set(leds_error_3);
            break;

        default:
            LEDS_Set(leds_error_5);
            break;
    }

    Panic_StopTasksAndWait();
}