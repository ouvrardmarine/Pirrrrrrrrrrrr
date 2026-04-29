/*
 * rtos_support.h
 *
 *  Created on: 18 mars 2026
 *      Author: Marine OUVRARS
 */

#ifndef RTOS_SUPPORT_H_
#define RTOS_SUPPORT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l0xx_ll_tim.h"
#include "application.h"
#include <cstdint>

/**
 * @brief Classe fournissant des fonctions de support RTOS et timer 32 bits basé sur TIM7
 */
class RtosSupport {
public:
    // Initialise TIM7 pour créer un timer 32 bits
    static void init();

    // Retourne le timer 32 bits basé sur TIM7
    static uint32_t getTimer();

    // Fonction pour la FreeRTOS PreSleepProcessing (tickless idle)
    static void preSleepProcessing(uint32_t* ulExpectedIdleTime);

    // Interrupt handler pour TIM7
    static void handleTIM7IRQ();

private:
    // Valeur supérieure du compteur 32 bits (simulation avec overflow 16 bits)
    static volatile uint16_t counter16bitUpper;
};

#ifdef __cplusplus
}
#endif

#endif /* RTOS_SUPPORT_H_ */
