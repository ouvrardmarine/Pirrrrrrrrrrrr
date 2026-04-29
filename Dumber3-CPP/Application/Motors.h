#ifndef MOTORS_H_
#define MOTORS_H_

#include <cstdint>
#include "stm32l0xx_hal.h"

class Motors
{
public:

    /* API publique */

    static void init();

    static void move(int32_t distance);

    static void turn(int32_t rotations);

    static void stop();

    static void handlerTask(void *params);

    static void controlTask(void *params);

    static void resetControlTask();

    /* callbacks timers */

    static void timerCaptureCallback(TIM_HandleTypeDef *htim);

private:

    /* constantes */

    static constexpr int16_t MAX_COMMAND = 200;

    /* structures internes */

    struct MotorState
    {
        int16_t output;
        int16_t setpoint;
        uint16_t encoder;
        uint16_t encoderEdge;
        uint8_t slowMotor;
    };

    struct DifferentialState
    {
        uint8_t type;
        int16_t output;
        int16_t setpoint;
        int32_t distance;
        int32_t turns;
    };

    /* états */

    static MotorState leftMotorState;
    static MotorState rightMotorState;
    static DifferentialState diffState;

    /* fonctions internes */

    static void set(int16_t leftMotor, int16_t rightMotor);

    static void powerOn();

    static void powerOff();

    static int16_t encoderCorrection(const MotorState& state);
};

/* callbacks HAL obligatoirement C */

extern "C"
{
    void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim);
}

#endif /* MOTORS_H_ */