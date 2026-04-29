/**
 ******************************************************************************
 * @file moteur.h
 * @brief motors driver header (C++ version)
 * @author S. DI MERCURIO (dimercur@insa-toulouse.fr)
 * @date December 2023
 *
 ******************************************************************************
 * @copyright Copyright 2023 INSA-GEI, Toulouse, France. All rights reserved.
 * @copyright This project is released under the Lesser GNU Public License (LGPL-3.0-only).
 *
 * @copyright This file is part of "Dumber" project
 *
 ******************************************************************************
 */

#ifndef MOTEUR_H
#define MOTEUR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "moteur.h"
#include "timers.h"
#include "stm32l0xx_ll_gpio.h"
#include "stm32l0xx_ll_tim.h"
#include <limits.h>

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup MOTORS
 * @{
 */

/**
 * @brief Class encapsulating the motors driver logic.
 *
 * Manages motor control, encoder feedback, and regulation loop for
 * the Dumber robot platform.
 */
class Moteur {
public:
    // -------------------------------------------------------------------------
    // Public API — same interface as the original C functions
    // -------------------------------------------------------------------------

    /**
     * @brief  Constructor: initializes the motor driver
     */
    Moteur();

    /**
     * @brief  Initialize motors driver (replaces MOTORS_Init)
     */
    void MOTORS_Init(void);

    /**
     * @brief  Request a movement in straight line (replaces MOTORS_Move)
     * @param[in] distance Distance in mm. Positive = forward, negative = backward.
     */
    void MOTORS_Move(int32_t distance);

    /**
     * @brief  Request a rotation movement (replaces MOTORS_Turn)
     * @param[in] rotations Angle in degrees. Positive = clockwise, negative = counterclockwise.
     */
    void MOTORS_Turn(int32_t rotations);

    /**
     * @brief  Stop any movement (replaces MOTORS_Stop)
     */
    void MOTORS_Stop(void);

    /**
     * @brief  Restart the control task (replaces MOTORS_ResetControlTask)
     */
    void MOTORS_ResetControlTask(void);

    /**
     * @brief  HAL callback for encoder capture interrupts (replaces HAL_TIM_IC_CaptureCallback)
     * @param[in] htim Pointer to the timer handle that triggered the interrupt
     */
    void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);

    /**
     * @brief  Overflow interrupt handler for encoder timers (replaces MOTORS_TimerEncodeurUpdate)
     * @param[in] htim Pointer to the timer handle that triggered the interrupt
     */
    void MOTORS_TimerEncodeurUpdate(TIM_HandleTypeDef *htim);

private:
    // -------------------------------------------------------------------------
    // Internal types
    // -------------------------------------------------------------------------

    /** Structure for storing per-motor control state */
    typedef struct {
        int16_t  output;       /**< Output value sent to motor [-32768..32767] */
        int16_t  setpoint;     /**< Setpoint for the control loop */
        uint16_t encoder;      /**< Raw encoder value from last control iteration */
        uint16_t encoderEdge;  /**< Delta-T between two encoder pulses */
        uint8_t  slowMotor;    /**< Flag: motor is considered halted */
    } MOTORS_MotorState;

    /** Structure for differential (trajectory) control state */
    typedef struct {
        uint8_t  type;      /**< Not used */
        int16_t  output;    /**< Not used */
        int16_t  setpoint;  /**< Not used */
        int32_t  distance;  /**< Remaining distance to travel */
        int32_t  turns;     /**< Remaining rotation count */
    } MOTORS_DifferentialState;

    /** Correction point associating a raw encoder value to a linearized value */
    typedef struct {
        uint16_t encoder;    /**< Raw encoder value */
        uint16_t correction; /**< Linearized equivalent */
    } MOTORS_CorrectionPoint;

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------

    static const uint16_t MOTORS_MAX_COMMAND          = 200;
    static const uint16_t MOTORS_MAX_ENCODER           = USHRT_MAX;
    static const int16_t  MOTOR_Kp                    = 300;
    static const uint8_t  MOTORS_MAX_CORRECTION_POINTS = 16;

    static const MOTORS_CorrectionPoint MOTORS_CorrectionPoints[MOTORS_MAX_CORRECTION_POINTS];

    // -------------------------------------------------------------------------
    // Motor state
    // -------------------------------------------------------------------------

    MOTORS_MotorState        MOTORS_LeftMotorState;
    MOTORS_MotorState        MOTORS_RightMotorState;
    MOTORS_DifferentialState MOTORS_DiffState;

    // -------------------------------------------------------------------------
    // FreeRTOS task objects
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskMotors;
    StackType_t   xStackMotors[STACK_SIZE];
    TaskHandle_t  xHandleMotors;

    StaticTask_t  xTaskMotorsControl;
    StackType_t   xStackMotorsControl[STACK_SIZE];
    TaskHandle_t  xHandleMotorsControl;

    // -------------------------------------------------------------------------
    // Private methods (internal helpers)
    // -------------------------------------------------------------------------

    /** Apply command values directly to the PWM outputs */
    void MOTORS_Set(int16_t leftMotor, int16_t rightMotor);

    /** Disable motor power supply */
    void MOTORS_PowerOff(void);

    /** Enable motor power supply */
    void MOTORS_PowerOn(void);

    /** Convert a raw encoder value to a linearized, signed speed value */
    int16_t MOTORS_EncoderCorrection(MOTORS_MotorState state);

    // -------------------------------------------------------------------------
    // FreeRTOS task trampolines (static wrappers required by FreeRTOS C API)
    // -------------------------------------------------------------------------

    /** Trampoline for MOTORS_HandlerTask */
    static void MOTORS_HandlerTask_trampoline(void *pvParameters);

    /** Trampoline for MOTORS_ControlTask */
    static void MOTORS_ControlTask_trampoline(void *pvParameters);

    /** Handler task: manages the mailbox and overall motor commands */
    void MOTORS_HandlerTask(void);

    /** Periodic control-loop task (3 ms) */
    void MOTORS_ControlTask(void);

#ifdef TESTS
    void Init_Systick(void);
    void StartMeasure(void);
    void EndMeasure(void);

    volatile uint32_t DEBUG_startTime;
    volatile uint32_t DEBUG_endTime;
    volatile uint32_t DEBUG_duration;
    volatile uint32_t DEBUG_worstCase;
#endif /* TESTS */
};

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MOTEUR_H */
