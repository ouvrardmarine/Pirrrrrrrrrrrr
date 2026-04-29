#include "battery_version4.hpp"
#include "stm32l0xx_ll_gpio.h"
#include "timers.h"

// ===== Static member definitions =====
TaskHandle_t Battery::taskHandle = nullptr;
TaskHandle_t Battery::adcWaitingTask = nullptr;

uint16_t Battery::adcRawValue = 0;
uint8_t Battery::buttonInactivity = 1;

StaticTask_t Battery::taskBuffer;
StackType_t Battery::stack[STACK_SIZE];

StaticTimer_t Battery::timerBuffer;
TimerHandle_t Battery::timerHandle = nullptr;

extern ADC_HandleTypeDef hadc;

// ===== Constants =====
#define BATTERY_MAX_ERROR 3

#define BATTERY_LEVEL_CRITICAL 135
#define BATTERY_LEVEL_LOW      145
#define BATTERY_LEVEL_HIGH     155

#define BATTERY_LEVEL_CHARGE_LOW  150
#define BATTERY_LEVEL_CHARGE_HIGH 170

// ===== Init =====
void Battery::Init() {
    taskHandle = xTaskCreateStatic(
        Thread,
        "BatteryTask",
        STACK_SIZE,
        nullptr,
        PriorityBatteryHandler,
        stack,
        &taskBuffer
    );

    timerHandle = xTimerCreateStatic(
        "ButtonTimer",
        pdMS_TO_TICKS(BUTTON_INACTIVITY_DELAY),
        pdFALSE, // FIXED (one-shot)
        nullptr,
        TimerCallback,
        &timerBuffer
    );

    xTimerStart(timerHandle, 0);
    vTaskResume(taskHandle);
}

// ===== Charger status =====
Battery::ChargerStatus Battery::GetChargerStatus() {
    uint32_t st2 = LL_GPIO_ReadInputPort(CHARGER_ST2_GPIO_Port) & CHARGER_ST2_Pin;
    uint32_t st1 = LL_GPIO_ReadInputPort(CHARGER_ST1_GPIO_Port) & CHARGER_ST1_Pin;

    if (st1 && st2) return ChargerStatus::NotPlugged;
    if (st1 && !st2) return ChargerStatus::ChargeComplete;
    if (!st1 && st2) return ChargerStatus::Charging;
    return ChargerStatus::Error;
}

// ===== ADC =====
int Battery::GetVoltage(uint16_t* val) {
    adcRawValue = 0;
    adcWaitingTask = xTaskGetCurrentTaskHandle();

    if (HAL_ADC_Start_IT(&hadc) != HAL_OK)
        return -1;

    uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

    if (notified == 1) {
        *val = adcRawValue;
    } else {
        return -2;
    }

    adcWaitingTask = nullptr;
    return 0;
}

// ===== Battery level =====
uint16_t Battery::BatteryLevel(uint8_t voltage, ChargerStatus status) {
    switch (status) {
        case ChargerStatus::ChargeComplete:
            return MSG_ID_BAT_CHARGE_COMPLETE;

        case ChargerStatus::Charging:
            if (voltage <= BATTERY_LEVEL_CHARGE_LOW)
                return MSG_ID_BAT_CHARGE_LOW;
            else if (voltage >= BATTERY_LEVEL_CHARGE_HIGH)
                return MSG_ID_BAT_CHARGE_HIGH;
            else
                return MSG_ID_BAT_CHARGE_MED;

        case ChargerStatus::NotPlugged:
            if (voltage <= BATTERY_LEVEL_CRITICAL)
                return MSG_ID_BAT_CRITICAL_LOW;
            else if (voltage <= BATTERY_LEVEL_LOW)
                return MSG_ID_BAT_LOW;
            else if (voltage >= BATTERY_LEVEL_HIGH)
                return MSG_ID_BAT_HIGH;
            else
                return MSG_ID_BAT_MED;

        default:
            return MSG_ID_BAT_CHARGE_ERR;
    }
}

// ===== Main thread =====
void Battery::Thread(void* params) {
    uint16_t voltage;
    uint8_t errorCnt = 0;
    TickType_t lastWake = xTaskGetTickCount();

    while (1) {
        if (GetVoltage(&voltage) == 0) {
            ChargerStatus status = GetChargerStatus();

            if (status == ChargerStatus::Error) {
                if (++errorCnt >= BATTERY_MAX_ERROR) {
                    MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BAT_CHARGE_ERR, 0, nullptr);
                }
            } else {
                errorCnt = 0; // FIXED
                uint16_t msg = BatteryLevel(voltage, status);
                MESSAGE_SendMailbox(APPLICATION_Mailbox, msg, 0, nullptr);
            }
        } else {
            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BAT_ADC_ERR, 0, nullptr);
        }

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(BATTERY_POLLING_DELAY));
    }
}

// ===== ADC ISR hook =====
void Battery::AdcConvCpltCallback(ADC_HandleTypeDef* hadc) {
    BaseType_t hpTaskWoken = pdFALSE;

    adcRawValue = HAL_ADC_GetValue(hadc);

    if (adcWaitingTask != nullptr) {
        vTaskNotifyGiveFromISR(adcWaitingTask, &hpTaskWoken);
        adcWaitingTask = nullptr;
        portYIELD_FROM_ISR(hpTaskWoken);
    }
}

// ===== Timer callback =====
void Battery::TimerCallback(TimerHandle_t xTimer) {
    buttonInactivity = 0;
}

// ===== GPIO EXTI =====
void Battery::GpioExtiCallback(uint16_t GPIO_Pin) {
    BaseType_t hpTaskWoken = pdFALSE;

    if (GPIO_Pin == BUTTON_SENSE_Pin) {
        if (!buttonInactivity) {
            if (HAL_GPIO_ReadPin(BUTTON_SENSE_GPIO_Port, GPIO_Pin) == GPIO_PIN_RESET) {
                MESSAGE_SendMailboxFromISR(
                    APPLICATION_Mailbox,
					MSG_ID_BUTTON_PRESSED,
                    0,
                    0,
                    &hpTaskWoken
                );
            }
        }
    }

    if (hpTaskWoken) {
        portYIELD_FROM_ISR(hpTaskWoken);
    }
}

// ===== C linkage wrappers (important for HAL) =====
extern "C" {

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *ha) {
    Battery::AdcConvCpltCallback(ha);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    Battery::GpioExtiCallback(GPIO_Pin);
}

}
