/*
 * rtos_supoort.cpp
 *
 *  Created on: 18 mars 2026
 *      Author: 33783
 */

#include "rtos_support.h"
//#include "stm32l0xx_hal.h"

// Définition du membre statique
volatile uint16_t RtosSupport::counter16bitUpper = 0;

void RtosSupport::init() {
    TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
    TIM_HandleTypeDef htim7;

    __HAL_RCC_TIM7_CLK_ENABLE();

    htim7.Instance = TIM7;
    htim7.Init.Prescaler = 0;
    htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim7.Init.Period = 65535;
    htim7.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
        Error_Handler();
    }

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim7, &sClockSourceConfig) != HAL_OK) {
        Error_Handler();
    }

    // TIM7 interrupt Init
    LL_TIM_EnableIT_UPDATE(TIM7);
    HAL_NVIC_SetPriority(TIM7_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);

    counter16bitUpper = 0;
    LL_TIM_EnableCounter(TIM7);
}

uint32_t RtosSupport::getTimer() {
    uint16_t currentVal = LL_TIM_GetCounter(TIM7);
    return (static_cast<uint32_t>(counter16bitUpper) << 16) + static_cast<uint32_t>(currentVal);
}

void RtosSupport::handleTIM7IRQ() {
    LL_TIM_ClearFlag_UPDATE(TIM7);
    counter16bitUpper++;
}

void RtosSupport::preSleepProcessing(uint32_t* ulExpectedIdleTime) {
    // Ne rien faire, permet d'activer tickless idle dans FreeRTOS
    (void)ulExpectedIdleTime;
}


