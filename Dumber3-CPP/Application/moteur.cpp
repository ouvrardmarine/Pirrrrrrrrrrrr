/**
 ******************************************************************************
 * @file moteur.cpp
 * @brief motors driver body (C++ version)
 * @author S. DI MERCURIO (dimercur@insa-toulouse.fr)
 * @date December 2023
 *
 ******************************************************************************
 * @copyright Copyright 2023 INSA-GEI, Toulouse, France. All rights reserved.
 * @copyright This project is released under the Lesser GNU Public License (LGPL-3.0-only).
 *
 * @copyright This file is part of "Dumber" project
 *
 * @copyright This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 ******************************************************************************
 */
#include "FreeRTOS.h"
#include "moteur.h"
#include "task.h"
#include "messages.h"
#include "mailbox.h"
#include "application.h"
#include "main.h"
#include "gpio.h"

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup MOTORS
 * Motors driver is in charge of controlling motors and applying a regulation to ensure a linear trajectory
 *
 * Global informations about peripherals
 * - Main clock: 6 Mhz
 * - TIM2 PWM Input (CH1): right encoder PHB : 0 -> 65535
 * - TIM21 PWM Input (CH1): left encoder PHA: 0 -> 65535
 * - TIM3: PWM Output motor (0->200) (~30 Khz)
 * @{
 */

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim21;
extern TIM_HandleTypeDef htim3;


// -------------------------------------------------------------------------
// Static correction-points table
// -------------------------------------------------------------------------

/**
 * Array of correction points, associating a raw encoder value and a linearized correction.
 *
 * Basically, encoders return delay between two signal edges. If motor is stopped, encoder
 * returns 65535 (MOTORS_MAX_ENCODER). The value tends towards zero as the motor accelerates.
 * The resulting encoder output is non-linear and inverted with respect to the setpoint.
 *
 * This table gives corresponding points, associating a raw, non-linear and inverted encoder
 * value to a linearized, corrected value that can be compared to the setpoint.
 */
const Moteur::MOTORS_CorrectionPoint
Moteur::MOTORS_CorrectionPoints[Moteur::MOTORS_MAX_CORRECTION_POINTS] = {
    { MOTORS_MAX_ENCODER - 1, 1     },
    { 42000,                  100   },
    { 22000,                  2500  },
    { 18000,                  5000  },
    { 16500,                  7500  },
    { 15500,                  10000 },
    { 14500,                  12500 },
    { 13000,                  15000 },
    { 12500,                  17500 },
    { 12200,                  20000 },
    { 11500,                  22500 },
    { 11100,                  25000 },
    { 11000,                  27500 },
    { 10900,                  29000 },
    { 10850,                  30500 },
    { 10800,                  SHRT_MAX } // 32767
};

// =========================================================================
// Constructor
// =========================================================================

Moteur::Moteur()
    : xHandleMotors(NULL), xHandleMotorsControl(NULL)
{
    /* Zero-initialise motor states */
    MOTORS_LeftMotorState  = { 0, 0, 0, 0, 0 };
    MOTORS_RightMotorState = { 0, 0, 0, 0, 0 };
    MOTORS_DiffState       = { 0, 0, 0, 0, 0 };

#ifdef TESTS
    DEBUG_startTime = 0;
    DEBUG_endTime   = 0;
    DEBUG_duration  = 0;
    DEBUG_worstCase = 0;
#endif
}

// =========================================================================
// FreeRTOS task trampolines
// =========================================================================

void Moteur::MOTORS_HandlerTask_trampoline(void *pvParameters) {
    static_cast<Moteur *>(pvParameters)->MOTORS_HandlerTask();
}

void Moteur::MOTORS_ControlTask_trampoline(void *pvParameters) {
    static_cast<Moteur *>(pvParameters)->MOTORS_ControlTask();
}

// =========================================================================
// Public API
// =========================================================================

/* +++ evoxx-probleme-refresh-wdt-moteurs-on : probleme lorsque l'on sort de l'etat WatchdogDisabled */
/**
 * @brief  Function for restarting motors control task
 *
 * @param  None
 * @return None
 */
void Moteur::MOTORS_ResetControlTask(void) {
    vTaskDelete(xHandleMotorsControl);

    /* Create the task without using any dynamic memory allocation. */
    xHandleMotorsControl = xTaskCreateStatic(
            MOTORS_ControlTask_trampoline,  /* Function that implements the task. */
            "MOTORS Control",               /* Text name for the task. */
            STACK_SIZE,                     /* Number of indexes in the xStack array. */
            this,                           /* Parameter passed into the task (pointer to instance). */
            PriorityMotorsAsservissement,   /* Priority at which the task is created. */
            xStackMotorsControl,            /* Array to use as the task's stack. */
            &xTaskMotorsControl);           /* Variable to hold the task's data structure. */
    vTaskSuspend(xHandleMotorsControl); // On ne lance la tache d'asservissement que lorsqu'une commande moteur arrive
}
/* --- evoxx-probleme-refresh-wdt-moteurs-on : probleme lorsque l'on sort de l'etat WatchdogDisabled */

/**
 * @brief  Function for initializing motors driver
 *
 * @param  None
 * @return None
 */
void Moteur::MOTORS_Init(void) {
    /* Désactive les alimentations des moteurs */
    MOTORS_PowerOff();

    /* Create the task without using any dynamic memory allocation. */
    xHandleMotors = xTaskCreateStatic(
            MOTORS_HandlerTask_trampoline,  /* Function that implements the task. */
            "MOTORS Handler",               /* Text name for the task. */
            STACK_SIZE,                     /* Number of indexes in the xStack array. */
            this,                           /* Parameter passed into the task (pointer to instance). */
            PriorityMotorsHandler,          /* Priority at which the task is created. */
            xStackMotors,                   /* Array to use as the task's stack. */
            &xTaskMotors);                  /* Variable to hold the task's data structure. */
    vTaskResume(xHandleMotors);

    /* Create the task without using any dynamic memory allocation. */
    xHandleMotorsControl = xTaskCreateStatic(
            MOTORS_ControlTask_trampoline,  /* Function that implements the task. */
            "MOTORS Control",               /* Text name for the task. */
            STACK_SIZE,                     /* Number of indexes in the xStack array. */
            this,                           /* Parameter passed into the task (pointer to instance). */
            PriorityMotorsAsservissement,   /* Priority at which the task is created. */
            xStackMotorsControl,            /* Array to use as the task's stack. */
            &xTaskMotorsControl);           /* Variable to hold the task's data structure. */
    vTaskSuspend(xHandleMotorsControl); // On ne lance la tache d'asservissement que lorsqu'une commande moteur arrive

    MOTORS_PowerOff();

#ifdef TESTS
    Init_Systick();
#endif /* TESTS */
}

/**
 * @brief  Request a movement in straight line
 *
 * @remark This function wraps a message sending to the motors mailbox.
 *         In case of multiple calls, only the last one will be applied,
 *         with potentially some spurious movement when processing previous messages.
 *
 * @param[in] distance Distance to move on, in mm.
 *            Positive values for forward movements and negative values for backward movements.
 * @return None
 */
void Moteur::MOTORS_Move(int32_t distance) {
    static int32_t dist;

    dist = distance * 15;

    if (dist) {
        MOTORS_PowerOn();
        MESSAGE_SendMailbox(MOTORS_Mailbox, MSG_ID_MOTORS_MOVE,
                APPLICATION_Mailbox, (void*) &dist);
    } else {
        MOTORS_Stop();
    }
}

/**
 * @brief  Request a movement in rotation
 *
 * @remark This function wraps a message sending to the motors mailbox.
 *         In case of multiple calls, only the last one will be applied,
 *         with potentially some spurious movement when processing previous messages.
 *
 * @param[in] rotations Angle of rotation to do in degrees.
 *            Positive values for clockwise rotations and negative values for counterclockwise rotations.
 * @return None
 */
void Moteur::MOTORS_Turn(int32_t rotations) {
    static int32_t turns;

    turns = rotations;

    if (turns) {
        MOTORS_PowerOn();
        MESSAGE_SendMailbox(MOTORS_Mailbox, MSG_ID_MOTORS_TURN,
                APPLICATION_Mailbox, (void*) &turns);
    } else {
        MOTORS_Stop();
    }
}

/**
 * @brief  Request for stopping any movement
 *
 * @remark This function wraps a message sending to the motors mailbox.
 *         In case of multiple calls, only the last one will be applied,
 *         with potentially some spurious movement when processing previous messages.
 *
 * @param  None
 * @return None
 */
void Moteur::MOTORS_Stop(void) {
    MOTORS_PowerOff();
    MESSAGE_SendMailbox(MOTORS_Mailbox, MSG_ID_MOTORS_STOP,
            APPLICATION_Mailbox, (void*) NULL);
}

// =========================================================================
// Private task implementations
// =========================================================================

/**
 * @brief Handler task for motor control
 *        Manages mailbox and overall motor management.
 *
 * @return None
 */
void Moteur::MOTORS_HandlerTask(void) {
    MESSAGE_Typedef msg;
    int32_t distance, tours;

    while (1) {
        msg = MESSAGE_ReadMailbox(MOTORS_Mailbox);

        switch (msg.id) {
        case MSG_ID_MOTORS_MOVE:
            distance = *((int32_t*) msg.data);
            MOTORS_DiffState.distance = distance;
            MOTORS_DiffState.turns    = 0;

            if (distance > 0) {
                MOTORS_LeftMotorState.setpoint  = 50;
                MOTORS_RightMotorState.setpoint = 50;
            } else {
                MOTORS_LeftMotorState.setpoint  = -50;
                MOTORS_RightMotorState.setpoint = -50;
            }

            vTaskResume(xHandleMotorsControl);
            break;

        case MSG_ID_MOTORS_TURN:
            tours = *((int32_t*) msg.data);
            MOTORS_DiffState.distance = 0;
            MOTORS_DiffState.turns    = tours;

            if (tours > 0) {
                MOTORS_LeftMotorState.setpoint  = -50;
                MOTORS_RightMotorState.setpoint =  50;
            } else {
                MOTORS_LeftMotorState.setpoint  =  50;
                MOTORS_RightMotorState.setpoint = -50;
            }

            vTaskResume(xHandleMotorsControl);
            break;

        case MSG_ID_MOTORS_STOP:
            MOTORS_DiffState.distance = 0;
            MOTORS_DiffState.turns    = 0;

            MOTORS_LeftMotorState.setpoint  = 0;
            MOTORS_RightMotorState.setpoint = 0;

            /* +++ evoxx-probleme-refresh-wdt-moteurs-on */
            MOTORS_ResetControlTask();
            /* --- evoxx-probleme-refresh-wdt-moteurs-on */

            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_MOTORS_END_OF_MOUVMENT,
                    MOTORS_Mailbox, (void*) NULL);
            break;

        default:
            break;
        }
    }
}

/**
 * @brief Control loop task
 *        Periodic task (3 ms) for the motor control loop.
 *
 * @remark Started by MOTORS_HandlerTask when a movement message is received;
 *         suspends itself when movement is finished.
 *
 * @return None
 */
void Moteur::MOTORS_ControlTask(void) {
    TickType_t xLastWakeTime;
    int16_t leftError, rightError = 0;
    int16_t leftEncoder, rightEncoder;
    int32_t locCmdG, locCmdD;

    /* Initialise xLastWakeTime with the current time. */
    xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        /* Wait for the next cycle. */
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(MOTORS_REGULATION_DELAY));

        leftEncoder  = MOTORS_EncoderCorrection(MOTORS_LeftMotorState);
        rightEncoder = MOTORS_EncoderCorrection(MOTORS_RightMotorState);

        /*
         * encodeur est entre -32768 et +32767, selon le sens de rotation du moteur
         * consigne est entre -32768 et +32767 selon le sens de rotation du moteur
         * erreur est entre -32768 et 32767 selon la différence à apporter à la commande
         */

        leftError  = MOTORS_LeftMotorState.setpoint  - leftEncoder;
        rightError = MOTORS_RightMotorState.setpoint - rightEncoder;

        if (((MOTORS_RightMotorState.setpoint == 0)
                && (MOTORS_LeftMotorState.setpoint == 0))
                && ((rightError == 0) && (leftError == 0)))
        {
            MOTORS_PowerOff();
            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_MOTORS_END_OF_MOUVMENT,
                    MOTORS_Mailbox, (void*) NULL);
            vTaskSuspend(xHandleMotorsControl);
        }

        /* ----- Left motor ----- */
        if (MOTORS_LeftMotorState.setpoint == 0) {
            MOTORS_LeftMotorState.output = 0;
        } else {
            if (leftError != 0) {
                locCmdG = ((int32_t) MOTOR_Kp * (int32_t) leftError) / 100;

                if (MOTORS_LeftMotorState.setpoint >= 0) {
                    if (locCmdG < 0)
                        MOTORS_LeftMotorState.output = 0;
                    else if (locCmdG > SHRT_MAX)
                        MOTORS_LeftMotorState.output = SHRT_MAX;
                    else
                        MOTORS_LeftMotorState.output = (int16_t) locCmdG;
                } else {
                    if (locCmdG > 0)
                        MOTORS_LeftMotorState.output = 0;
                    else if (locCmdG < SHRT_MIN)
                        MOTORS_LeftMotorState.output = SHRT_MIN;
                    else
                        MOTORS_LeftMotorState.output = (int16_t) locCmdG;
                }
            }
        }

        /* ----- Right motor ----- */
        if (MOTORS_RightMotorState.setpoint == 0) {
            MOTORS_RightMotorState.output = 0;
        } else {
            if (rightError != 0) {
                locCmdD = ((int32_t) MOTOR_Kp * (int32_t) rightError) / 100;

                if (MOTORS_RightMotorState.setpoint >= 0) {
                    if (locCmdD < 0)
                        MOTORS_RightMotorState.output = 0;
                    else if (locCmdD > SHRT_MAX)
                        MOTORS_RightMotorState.output = SHRT_MAX;
                    else
                        MOTORS_RightMotorState.output = (int16_t) locCmdD;
                } else {
                    if (locCmdD > 0)
                        MOTORS_RightMotorState.output = 0;
                    else if (locCmdD < SHRT_MIN)
                        MOTORS_RightMotorState.output = SHRT_MIN;
                    else
                        MOTORS_RightMotorState.output = (int16_t) locCmdD;
                }
            }
        }

        /* Apply commands to motors */
        MOTORS_Set(MOTORS_LeftMotorState.output, MOTORS_RightMotorState.output);
    }
}

// =========================================================================
// Private helpers
// =========================================================================

/**
 * @brief Function for converting raw encoder values to linearized values
 *
 * @remark This function uses MOTORS_CorrectionPoints for its conversion.
 *
 * @param[in] state Current state of a motor, including raw encoder value
 * @return Linearized value, from -32768 (full backward) to 32767 (full forward)
 */
int16_t Moteur::MOTORS_EncoderCorrection(MOTORS_MotorState state) {
    int16_t  correction = 0;
    uint8_t  index      = 0;
    uint32_t A, B, C;
    uint16_t encoder = state.encoder;

    if (encoder == MOTORS_MAX_ENCODER) {
        correction = 0;
    } else {
        /* Binary search for the matching interval */
        while (index < MOTORS_MAX_CORRECTION_POINTS) {
            if ((MOTORS_CorrectionPoints[index].encoder >= encoder)
                    && (MOTORS_CorrectionPoints[index + 1].encoder < encoder)) {
                break; /* interval found */
            } else {
                index++;
            }
        }

        if (index >= MOTORS_MAX_CORRECTION_POINTS) {
            correction = SHRT_MAX;
        } else {
            A = encoder - MOTORS_CorrectionPoints[index + 1].encoder;
            B = MOTORS_CorrectionPoints[index + 1].correction
                    - MOTORS_CorrectionPoints[index].correction;
            C = MOTORS_CorrectionPoints[index].encoder
                    - MOTORS_CorrectionPoints[index + 1].encoder;

            correction = (int16_t)(MOTORS_CorrectionPoints[index + 1].correction
                    - (uint16_t)((A * B) / C));
        }
    }

    /*
     * Selon le sens de rotation du moteur (commande > 0 ou < 0),
     * on corrige le signe du capteur.
     */
    if (state.setpoint < 0)
        correction = -correction;

    return correction;
}

/**
 * @brief  Power off motors, disabling power regulator
 *
 * @param  None
 * @return None
 */
void Moteur::MOTORS_PowerOff(void) {
    LL_TIM_DisableCounter(TIM3);
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_DisableCounter(TIM21);

    LL_TIM_CC_DisableChannel(TIM3,
            LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2
            | LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH4);

    LL_TIM_CC_DisableChannel(TIM2,  LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);
    LL_TIM_CC_DisableChannel(TIM21, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);

    LL_TIM_DisableIT_CC1(TIM2);
    LL_TIM_DisableIT_CC1(TIM21);
    LL_TIM_DisableIT_UPDATE(TIM2);
    LL_TIM_DisableIT_UPDATE(TIM21);

    LL_GPIO_SetOutputPin(GPIOB, SHUTDOWN_ENCODERS_Pin);
    LL_GPIO_ResetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

/**
 * @brief  Power on motors, enabling power regulator
 *
 * @param  None
 * @return None
 */
void Moteur::MOTORS_PowerOn(void) {
    LL_TIM_EnableCounter(TIM3);
    LL_TIM_EnableCounter(TIM2);
    LL_TIM_EnableCounter(TIM21);

    LL_TIM_CC_EnableChannel(TIM3,
            LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2
            | LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH4);

    LL_TIM_CC_EnableChannel(TIM2,  LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);
    LL_TIM_CC_EnableChannel(TIM21, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2);

    LL_TIM_EnableIT_CC1(TIM2);
    LL_TIM_EnableIT_CC1(TIM21);
    LL_TIM_EnableIT_UPDATE(TIM2);
    LL_TIM_EnableIT_UPDATE(TIM21);

    LL_GPIO_ResetOutputPin(GPIOB, SHUTDOWN_ENCODERS_Pin);
    LL_GPIO_SetOutputPin(GPIOB, SHUTDOWN_5V_Pin);
}

/**
 * @brief Set command to motors
 *
 * Drives motors directly. Values applied are outputs from the regulation law.
 * If the power supply is not enabled, this function will enable it.
 *
 * Input values are in [-SHRT_MAX; SHRT_MAX], mapped to [-MOTORS_MAX_COMMAND; +MOTORS_MAX_COMMAND].
 *
 * @param[in] leftMotor  Requested command for left motor
 * @param[in] rightMotor Requested command for right motor
 * @return None
 */
void Moteur::MOTORS_Set(int16_t leftMotor, int16_t rightMotor) {
    int32_t leftValue, rightValue;

    leftValue  = (int32_t)(((int32_t) leftMotor  * (int32_t) SHRT_MAX)
                            / (int32_t) MOTORS_MAX_COMMAND);
    rightValue = (int32_t)(((int32_t) rightMotor * (int32_t) SHRT_MAX)
                            / (int32_t) MOTORS_MAX_COMMAND);

    if (LL_GPIO_IsOutputPinSet(GPIOB, SHUTDOWN_5V_Pin) == GPIO_PIN_RESET)
        MOTORS_PowerOn();

    /* Right motor */
    if (rightMotor >= 0) {
        LL_TIM_OC_SetCompareCH2(TIM3, (uint32_t) rightValue);
        LL_TIM_OC_SetCompareCH1(TIM3, (uint32_t) 0);
    } else {
        LL_TIM_OC_SetCompareCH2(TIM3, (uint32_t) 0);
        LL_TIM_OC_SetCompareCH1(TIM3, (uint32_t) rightValue);
    }

    /* Left motor */
    if (leftMotor >= 0) {
        LL_TIM_OC_SetCompareCH4(TIM3, (uint32_t) leftValue);
        LL_TIM_OC_SetCompareCH3(TIM3, (uint32_t) 0);
    } else {
        LL_TIM_OC_SetCompareCH4(TIM3, (uint32_t) 0);
        LL_TIM_OC_SetCompareCH3(TIM3, (uint32_t) leftValue);
    }
}

// =========================================================================
// Interrupt callbacks
// =========================================================================

/**
 * @brief Get raw values from encoders and store them in corresponding motor state
 *
 * @remark Encoder values are timer counts representing time between two encoder pulses.
 *         Also manages distance and turn counters used for overall motor control.
 *
 * @param[in] htim Pointer to the timer handle that triggered the interrupt
 * @return None
 */
void Moteur::HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM21) { /* left motor */
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            if (MOTORS_LeftMotorState.slowMotor != 0) {
                MOTORS_LeftMotorState.encoder     = MOTORS_MAX_ENCODER;
                MOTORS_LeftMotorState.encoderEdge = MOTORS_MAX_ENCODER;
            } else {
                MOTORS_LeftMotorState.encoder     = (uint16_t) LL_TIM_IC_GetCaptureCH1(TIM21);
                MOTORS_LeftMotorState.encoderEdge = (uint16_t) LL_TIM_IC_GetCaptureCH2(TIM21);
            }

            if (LL_TIM_IsActiveFlag_UPDATE(TIM21))
                LL_TIM_ClearFlag_UPDATE(TIM21);

            MOTORS_LeftMotorState.slowMotor = 0;

            if (MOTORS_DiffState.distance) {
                if (MOTORS_DiffState.distance > 0) MOTORS_DiffState.distance--;
                else                               MOTORS_DiffState.distance++;

                if (MOTORS_DiffState.distance == 0) {
                    MOTORS_LeftMotorState.setpoint  = 0;
                    MOTORS_RightMotorState.setpoint = 0;
                }
            }

            if (MOTORS_DiffState.turns) {
                if (MOTORS_DiffState.turns > 0) MOTORS_DiffState.turns--;
                else                            MOTORS_DiffState.turns++;

                if (MOTORS_DiffState.turns == 0) {
                    MOTORS_LeftMotorState.setpoint  = 0;
                    MOTORS_RightMotorState.setpoint = 0;
                }
            }
        }
    } else { /* right motor */
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            if (MOTORS_RightMotorState.slowMotor != 0) {
                MOTORS_RightMotorState.encoder     = MOTORS_MAX_ENCODER;
                MOTORS_RightMotorState.encoderEdge = MOTORS_MAX_ENCODER;
            } else {
                MOTORS_RightMotorState.encoder     = (uint16_t) LL_TIM_IC_GetCaptureCH1(TIM2);
                MOTORS_RightMotorState.encoderEdge = (uint16_t) LL_TIM_IC_GetCaptureCH2(TIM2);
            }

            if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
                LL_TIM_ClearFlag_UPDATE(TIM2);

            MOTORS_RightMotorState.slowMotor = 0;
        }
    }
}

/**
 * @brief "Overflow" interrupt handler
 *
 *        When two "overflow" interrupts occur without an encoder interrupt in between,
 *        the motor is considered halted; encoder value is set to MOTORS_MAX_ENCODER.
 *
 * @param[in] htim Pointer to the timer handle that triggered the interrupt
 * @return None
 */
void Moteur::MOTORS_TimerEncodeurUpdate(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM21) { /* left motor */
        if ((MOTORS_LeftMotorState.slowMotor++) >= 1) {
            MOTORS_LeftMotorState.encoder   = MOTORS_MAX_ENCODER;
            MOTORS_LeftMotorState.slowMotor = 1;
        }
    } else { /* right motor */
        if ((MOTORS_RightMotorState.slowMotor++) >= 1) {
            MOTORS_RightMotorState.encoder   = MOTORS_MAX_ENCODER;
            MOTORS_RightMotorState.slowMotor = 1;
        }
    }
}

// =========================================================================
// Test helpers (compiled only when TESTS is defined)
// =========================================================================

#ifdef TESTS
void Moteur::Init_Systick(void)  { /* see original for commented-out impl */ }
void Moteur::StartMeasure(void)  { /* see original for commented-out impl */ }
void Moteur::EndMeasure(void)    { /* see original for commented-out impl */ }
#endif /* TESTS */

/** @} */
/** @} */
