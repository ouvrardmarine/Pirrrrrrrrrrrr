#include "Motors.h"

#include "config.h"    // Pour STACK_SIZE, PriorityMotorsHandler, etc.
#include "messages.h"  // Pour MESSAGE_SendMailbox, MOTORS_Mailbox, etc.
#include "main.h"      // Pour SHUTDOWN_5V_Pin et les autres defines de pins
#include "FreeRTOS.h"
#include "timers.h"
#include "stm32l0xx_ll_gpio.h"
#include "stm32l0xx_ll_tim.h"

#include <climits>

/* timers extern */

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim21;
extern TIM_HandleTypeDef htim3;

/* états statiques */

Motors::MotorState Motors::leftMotorState{};
Motors::MotorState Motors::rightMotorState{};
Motors::DifferentialState Motors::diffState{};

/* FreeRTOS */

StaticTask_t xTaskMotors;
StackType_t xStackMotors[STACK_SIZE];
TaskHandle_t xHandleMotors = nullptr;

StaticTask_t xTaskMotorsControl;
StackType_t xStackMotorsControl[STACK_SIZE];
TaskHandle_t xHandleMotorsControl = nullptr;

/* -------------------------------------------------------------------------- */
/* INIT */
/* -------------------------------------------------------------------------- */

void Motors::init()
{
    powerOff();

    xHandleMotors = xTaskCreateStatic(
        handlerTask,
        "MOTORS Handler",
        STACK_SIZE,
        nullptr,
        PriorityMotorsHandler,
        xStackMotors,
        &xTaskMotors);

    vTaskResume(xHandleMotors);

    xHandleMotorsControl = xTaskCreateStatic(
        controlTask,
        "MOTORS Control",
        STACK_SIZE,
        nullptr,
        PriorityMotorsAsservissement,
        xStackMotorsControl,
        &xTaskMotorsControl);

    vTaskSuspend(xHandleMotorsControl);
}

/* -------------------------------------------------------------------------- */

void Motors::move(int32_t distance)
{
    static int32_t dist;

    dist = distance * 15;

    if (dist)
    {
        powerOn();

        MESSAGE_SendMailbox(
            MOTORS_Mailbox,
            MSG_ID_MOTORS_MOVE,
            APPLICATION_Mailbox,
            &dist);
    }
    else
    {
        stop();
    }
}

/* -------------------------------------------------------------------------- */

void Motors::turn(int32_t rotations)
{
    static int32_t turns;

    turns = rotations;

    if (turns)
    {
        powerOn();

        MESSAGE_SendMailbox(
            MOTORS_Mailbox,
            MSG_ID_MOTORS_TURN,
            APPLICATION_Mailbox,
            &turns);
    }
    else
    {
        stop();
    }
}

/* -------------------------------------------------------------------------- */

void Motors::stop()
{
    powerOff();

    MESSAGE_SendMailbox(
        MOTORS_Mailbox,
        MSG_ID_MOTORS_STOP,
        APPLICATION_Mailbox,
        nullptr);
}

/* -------------------------------------------------------------------------- */
/* HANDLER TASK */
/* -------------------------------------------------------------------------- */

void Motors::handlerTask(void *params)
{
    MESSAGE_Typedef msg;

    while (1)
    {
        msg = MESSAGE_ReadMailbox(MOTORS_Mailbox);

        switch (msg.id)
        {

        case MSG_ID_MOTORS_MOVE:
        {
            int32_t distance = *static_cast<int32_t*>(msg.data);

            diffState.distance = distance;
            diffState.turns = 0;

            if (distance > 0)
            {
                leftMotorState.setpoint = 50;
                rightMotorState.setpoint = 50;
            }
            else
            {
                leftMotorState.setpoint = -50;
                rightMotorState.setpoint = -50;
            }

            vTaskResume(xHandleMotorsControl);
        }
        break;

        case MSG_ID_MOTORS_STOP:

            diffState.distance = 0;
            diffState.turns = 0;

            leftMotorState.setpoint = 0;
            rightMotorState.setpoint = 0;

            resetControlTask();

            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* PWM SET */
/* -------------------------------------------------------------------------- */

void Motors::set(int16_t leftMotor, int16_t rightMotor)
{
    int32_t leftValue =
        (static_cast<int32_t>(leftMotor) * SHRT_MAX) / MAX_COMMAND;

    int32_t rightValue =
        (static_cast<int32_t>(rightMotor) * SHRT_MAX) / MAX_COMMAND;

    if (rightMotor >= 0)
    {
        LL_TIM_OC_SetCompareCH2(TIM3, rightValue);
        LL_TIM_OC_SetCompareCH1(TIM3, 0);
    }
    else
    {
        LL_TIM_OC_SetCompareCH2(TIM3, 0);
        LL_TIM_OC_SetCompareCH1(TIM3, rightValue);
    }

    if (leftMotor >= 0)
    {
        LL_TIM_OC_SetCompareCH4(TIM3, leftValue);
        LL_TIM_OC_SetCompareCH3(TIM3, 0);
    }
    else
    {
        LL_TIM_OC_SetCompareCH4(TIM3, 0);
        LL_TIM_OC_SetCompareCH3(TIM3, leftValue);
    }
}

/* -------------------------------------------------------------------------- */
/* POWER */
/* -------------------------------------------------------------------------- */

void Motors::powerOn()
{
    LL_TIM_EnableCounter(TIM3);
    LL_GPIO_SetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

void Motors::powerOff()
{
    LL_TIM_DisableCounter(TIM3);
    LL_GPIO_ResetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

/* -------------------------------------------------------------------------- */
/* CALLBACK HAL */
/* -------------------------------------------------------------------------- */

extern "C" void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    Motors::timerCaptureCallback(htim);
}

void Motors::timerCaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM21)
    {
        leftMotorState.encoder =
            static_cast<uint16_t>(
                LL_TIM_IC_GetCaptureCH1(TIM21));
    }
    else
    {
        rightMotorState.encoder =
            static_cast<uint16_t>(
                LL_TIM_IC_GetCaptureCH1(TIM2));
    }
}
