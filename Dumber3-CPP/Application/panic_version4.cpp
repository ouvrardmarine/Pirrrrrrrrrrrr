#include "panic_version4.hpp"
#include "application.h"
#include "Motors.h"
#include "battery_version4.hpp"
#include "timers.h"
#include "leds.h"
#include "stm32l0xx_hal.h"

[[noreturn]] static void Panic_StopTasksAndWait(void)
{
    Application& app = Application::Instance();

    // Suspend subsystem tasks via their class APIs
    app.GetXbee().suspend();   // suspends xHandleXbeeRX + xHandleXbeeTXHandler
    Battery::suspend();        // suspends xHandleBattery
    Motors::suspend();         // suspends xHandleMotors + xHandleMotorsControl
    Motors::powerOff();        // cuts motor power (timers + GPIO)

    // Disable XBee radio
    HAL_GPIO_WritePin(XBEE_RESET_GPIO_Port, XBEE_RESET_Pin, GPIO_PIN_RESET);

    // Suspend application task, then self
    app.suspend();
    vTaskSuspend(xTaskGetCurrentTaskHandle());

    while (true) { __WFE(); }
}

[[noreturn]] void Panic_Raise(PanicType panicId)
{
    Application& app = Application::Instance();

    switch (panicId) {
        case PanicType::AdcError:     app.GetLeds().LEDS_Set(Leds::leds_error_1); break;
        case PanicType::ChargerError: app.GetLeds().LEDS_Set(Leds::leds_error_2); break;
        case PanicType::MallocError:  app.GetLeds().LEDS_Set(Leds::leds_error_3); break;
        default:                      app.GetLeds().LEDS_Set(Leds::leds_error_5); break;
    }

    Panic_StopTasksAndWait();
}
