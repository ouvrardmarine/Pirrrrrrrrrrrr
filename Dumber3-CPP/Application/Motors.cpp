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
    LL_TIM_EnableCounter(TIM2);
    LL_TIM_EnableCounter(TIM21);
    LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2|LL_TIM_CHANNEL_CH3|LL_TIM_CHANNEL_CH4);
    LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2);
    LL_TIM_CC_EnableChannel(TIM21, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2);
    LL_TIM_EnableIT_CC1(TIM2);
    LL_TIM_EnableIT_CC1(TIM21);
    LL_TIM_EnableIT_UPDATE(TIM2);
    LL_TIM_EnableIT_UPDATE(TIM21);
    LL_GPIO_ResetOutputPin(GPIOB, SHUTDOWN_ENCODERS_Pin);
    LL_GPIO_SetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

void Motors::powerOff()
{
    LL_TIM_DisableCounter(TIM3);
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM21);
    LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2|LL_TIM_CHANNEL_CH3|LL_TIM_CHANNEL_CH4);
    LL_TIM_CC_DisableChannel(TIM2, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2);
    LL_TIM_CC_DisableChannel(TIM21, LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2);
    LL_TIM_DisableIT_CC1(TIM2);
    LL_TIM_DisableIT_CC1(TIM21);
    LL_TIM_DisableIT_UPDATE(TIM2);
    LL_TIM_DisableIT_UPDATE(TIM21);
    LL_GPIO_SetOutputPin(GPIOB, SHUTDOWN_ENCODERS_Pin);
    LL_GPIO_ResetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

void Motors::suspend() {
    if (xHandleMotors)        vTaskSuspend(xHandleMotors);
    if (xHandleMotorsControl) vTaskSuspend(xHandleMotorsControl);
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
    if (htim->Instance == TIM21) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            if (leftMotorState.slowMotor != 0) {
                leftMotorState.encoder = USHRT_MAX;
                leftMotorState.encoderEdge = USHRT_MAX;
            } else {
                leftMotorState.encoder  = (uint16_t)LL_TIM_IC_GetCaptureCH1(TIM21);
                leftMotorState.encoderEdge = (uint16_t)LL_TIM_IC_GetCaptureCH2(TIM21);
            }
            if (LL_TIM_IsActiveFlag_UPDATE(TIM21)) LL_TIM_ClearFlag_UPDATE(TIM21);
            leftMotorState.slowMotor = 0;

            if (diffState.distance) {
                if (diffState.distance > 0) diffState.distance--;
                else diffState.distance++;
                if (diffState.distance == 0) { leftMotorState.setpoint = 0; rightMotorState.setpoint = 0; }
            }
            if (diffState.turns) {
                if (diffState.turns > 0) diffState.turns--;
                else diffState.turns++;
                if (diffState.turns == 0) { leftMotorState.setpoint = 0; rightMotorState.setpoint = 0; }
            }
        }
    } else {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            if (rightMotorState.slowMotor != 0) {
                rightMotorState.encoder = USHRT_MAX;
                rightMotorState.encoderEdge = USHRT_MAX;
            } else {
                rightMotorState.encoder  = (uint16_t)LL_TIM_IC_GetCaptureCH1(TIM2);
                rightMotorState.encoderEdge = (uint16_t)LL_TIM_IC_GetCaptureCH2(TIM2);
            }
            if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) LL_TIM_ClearFlag_UPDATE(TIM2);
            rightMotorState.slowMotor = 0;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* CALLBACK HAL */
/* -------------------------------------------------------------------------- */

// ... (ton code timerCaptureCallback existant) ...

/* -------------------------------------------------------------------------- */
/* CONSTANTES REGULATION */                          // ← AJOUTER ICI
/* -------------------------------------------------------------------------- */

#define MOTOR_Kp 300

struct CorrectionPoint { uint16_t encoder; uint16_t correction; };
static const CorrectionPoint correctionPoints[16] = {
    { 65534, 1 }, { 42000, 100 }, { 22000, 2500 }, { 18000, 5000 },
    { 16500, 7500 }, { 15500, 10000 }, { 14500, 12500 }, { 13000, 15000 },
    { 12500, 17500 }, { 12200, 20000 }, { 11500, 22500 }, { 11100, 25000 },
    { 11000, 27500 }, { 10900, 29000 }, { 10850, 30500 }, { 10800, 32767 }
};

/* -------------------------------------------------------------------------- */
/* ENCODER CORRECTION */
/* -------------------------------------------------------------------------- */


int16_t Motors::encoderCorrection(const MotorState& state)
{
    uint16_t encoder = state.encoder;
    int16_t correction = 0;
    uint8_t index = 0;

    if (encoder == USHRT_MAX)
        return 0;

    while (index < 16) {
        if ((correctionPoints[index].encoder >= encoder) &&
            (correctionPoints[index + 1].encoder < encoder))
            break;
        else
            index++;
    }

    if (index >= 16)
        correction = SHRT_MAX;
    else {
        uint32_t A = encoder - correctionPoints[index + 1].encoder;
        uint32_t B = correctionPoints[index + 1].correction - correctionPoints[index].correction;
        uint32_t C = correctionPoints[index].encoder - correctionPoints[index + 1].encoder;
        correction = (int16_t)(correctionPoints[index + 1].correction - (uint16_t)((A * B) / C));
    }

    if (state.setpoint < 0)
        correction = -correction;

    return correction;
}

/* -------------------------------------------------------------------------- */
/* RESET CONTROL TASK */
/* -------------------------------------------------------------------------- */

void Motors::resetControlTask()
{
    vTaskDelete(xHandleMotorsControl);

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
/* CONTROL TASK */
/* -------------------------------------------------------------------------- */

void Motors::controlTask(void *params)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int16_t leftError, rightError;
    int32_t locCmdG, locCmdD;

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MOTORS_REGULATION_DELAY));

        int16_t leftEncoder  = encoderCorrection(leftMotorState);
        int16_t rightEncoder = encoderCorrection(rightMotorState);

        leftError  = leftMotorState.setpoint  - leftEncoder;
        rightError = rightMotorState.setpoint - rightEncoder;

        if ((leftMotorState.setpoint == 0) && (rightMotorState.setpoint == 0)
            && (leftError == 0) && (rightError == 0))
        {
            powerOff();
            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_MOTORS_END_OF_MOUVMENT,
                MOTORS_Mailbox, nullptr);
            vTaskSuspend(xHandleMotorsControl);
        }

        // Moteur gauche
        if (leftMotorState.setpoint == 0)
            leftMotorState.output = 0;
        else if (leftError != 0) {
            locCmdG = ((int32_t)MOTOR_Kp * (int32_t)leftError) / 100;
            if (leftMotorState.setpoint >= 0)
                leftMotorState.output = (int16_t)((locCmdG < 0) ? 0 : (locCmdG > SHRT_MAX ? SHRT_MAX : locCmdG));
            else
                leftMotorState.output = (int16_t)((locCmdG > 0) ? 0 : (locCmdG < SHRT_MIN ? SHRT_MIN : locCmdG));
        }

        // Moteur droit
        if (rightMotorState.setpoint == 0)
            rightMotorState.output = 0;
        else if (rightError != 0) {
            locCmdD = ((int32_t)MOTOR_Kp * (int32_t)rightError) / 100;
            if (rightMotorState.setpoint >= 0)
                rightMotorState.output = (int16_t)((locCmdD < 0) ? 0 : (locCmdD > SHRT_MAX ? SHRT_MAX : locCmdD));
            else
                rightMotorState.output = (int16_t)((locCmdD > 0) ? 0 : (locCmdD < SHRT_MIN ? SHRT_MIN : locCmdD));
        }

        set(leftMotorState.output, rightMotorState.output);
    }
}
