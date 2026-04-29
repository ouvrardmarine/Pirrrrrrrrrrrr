/**
 ******************************************************************************
 * @file leds.h
 * @brief leds driver header (C++ version)
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

#ifndef LEDS_H
#define LEDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "config.h"

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup LEDS
 * Leds handler is in charge of leds animation.
 *
 * Leds module consists of two threads:
 * - LEDS_HandlerThread: waits for messages in the mailbox from the application.
 *   Depending on the message received, animation is started, modified or stopped.
 * - LEDS_ActionThread: periodic task in charge of animating leds with configured
 *   sprites for the given animation.
 * @{
 */

/**
 * @brief Class encapsulating the LED animation driver.
 *
 * Manages a 7-segment display driven by GPIO, including sprite tables,
 * FreeRTOS handler and action tasks.
 */
class Leds {
public:
    // -------------------------------------------------------------------------
    // Public API — same interface as the original C functions
    // -------------------------------------------------------------------------
	 /** Enumeration class defining possible leds animations */
	    typedef enum {
	    	leds_off=0,					/**< No animation */
	    	leds_idle,					/**< Idle animation (only dot point blinking) */
	    	leds_run,					/**< Run animation (leds animate in circle) */
	    	leds_run_with_watchdog,		/**< Run with watchdog animation (leds animate in circle, with dot point blinking)  */
	    	leds_bat_critical_low,		/**< Critical low bat animation (C,L and B lettres) */
	    	leds_bat_low,				/**< Low bat animation */
	    	leds_bat_med,				/**< Medium charged bat animation */
	    	leds_bat_high,				/**< Full charged bat animation */
	    	leds_bat_charge_low,		/**< Charge in progress (low bat level) animation */
	    	leds_bat_charge_med,		/**< Charge in progress (medium bat level) animation */
	    	leds_bat_charge_high,		/**< Charge in progress (high bat level) animation */
	    	leds_bat_charge_complete,	/**< Charge complete animation */
	    	leds_watchdog_expired,		/**< Watchdog expired animation (squares moving) */
	    	leds_error_1,				/**< Error 1 animation */
	    	leds_error_2,				/**< Error 2 animation */
	    	leds_error_3,				/**< Error 3 animation */
	    	leds_error_4,				/**< Error 4 animation */
	    	leds_error_5,				/**< Error 5 animation */
	    	leds_state_unknown			/**< Unknown animation */
	    } LEDS_State;

    /**
     * @brief Constructor: zero-initialises internal state
     */
    Leds();

    /**
     * @brief  Initialize LED animation (replaces LEDS_Init)
     */
    void LEDS_Init(void);

    /**
     * @brief  Request an animation (replaces LEDS_Set)
     * @param[in] state LED animation to request
     */
    void LEDS_Set(LEDS_State state);

    /**
     * @brief  Show a pattern directly — used during hard fault (replaces LEDS_HardFault)
     */
    void LEDS_HardFault(void);

private:
    // -------------------------------------------------------------------------
    // Pattern index constants
    // -------------------------------------------------------------------------

    static const uint8_t LED_PATTERN_ALL_OFF                  = 0;
    static const uint8_t LED_PATTERN_BAT_SPRITE_0             = 1;
    static const uint8_t LED_PATTERN_BAT_SPRITE_1             = 2;
    static const uint8_t LED_PATTERN_BAT_SPRITE_2             = 3;
    static const uint8_t LED_PATTERN_BAT_SPRITE_3             = 4;
    static const uint8_t LED_PATTERN_IDLE_0                   = 5;
    static const uint8_t LED_PATTERN_IDLE_1                   = 6;
    static const uint8_t LED_PATTERN_RUN_0                    = 7;
    static const uint8_t LED_PATTERN_RUN_1                    = 8;
    static const uint8_t LED_PATTERN_RUN_2                    = 9;
    static const uint8_t LED_PATTERN_RUN_3                    = 10;
    static const uint8_t LED_PATTERN_RUN_4                    = 11;
    static const uint8_t LED_PATTERN_RUN_5                    = 12;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_0      = 13;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_1      = 14;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_2      = 15;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_3      = 16;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_4      = 17;
    static const uint8_t LED_PATTERN_RUN_WITH_WATCHDOG_5      = 18;
    static const uint8_t LED_PATTERN_ERROR                    = 19;
    static const uint8_t LED_PATTERN_BATTERY                  = 20;
    static const uint8_t LED_PATTERN_DIGIT_0                  = 21;
    static const uint8_t LED_PATTERN_DIGIT_1                  = 22;
    static const uint8_t LED_PATTERN_DIGIT_2                  = 23;
    static const uint8_t LED_PATTERN_DIGIT_3                  = 24;
    static const uint8_t LED_PATTERN_DIGIT_4                  = 25;
    static const uint8_t LED_PATTERN_DIGIT_5                  = 26;
    static const uint8_t LED_PATTERN_DIGIT_6                  = 27;
    static const uint8_t LED_PATTERN_DIGIT_7                  = 28;
    static const uint8_t LED_PATTERN_DIGIT_8                  = 29;
    static const uint8_t LED_PATTERN_DIGIT_9                  = 30;
    static const uint8_t LED_PATTERN_DIGIT_C                  = 31;
    static const uint8_t LED_PATTERN_DIGIT_L                  = 32;
    static const uint8_t LED_PATTERN_DIGIT_B                  = 33;
    static const uint8_t LED_PATTERN_WDT_EXP_1                = 34;
    static const uint8_t LED_PATTERN_WDT_EXP_2                = 35;
    static const uint8_t LED_PATTERN_DIGIT_UNKNOWN            = 36;
    static const uint8_t LED_MAX_PATTERNS                     = 37;

    // -------------------------------------------------------------------------
    // Sprite table (GPIOA ON / GPIOB ON / GPIOA OFF / GPIOB OFF)
    // -------------------------------------------------------------------------

    static const uint16_t LEDS_Patterns[37][4];

    // -------------------------------------------------------------------------
    // Animation state
    // -------------------------------------------------------------------------

    LEDS_State LEDS_Animation;
    LEDS_State LEDS_AnimationAncien;

    // -------------------------------------------------------------------------
    // FreeRTOS handler task
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskLedsHandler;
    StackType_t   xStackLedsHandler[STACK_SIZE];
    TaskHandle_t  xHandleLedsHandler;

    // -------------------------------------------------------------------------
    // FreeRTOS action task
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskLedsAction;
    StackType_t   xStackLedsAction[STACK_SIZE];
    TaskHandle_t  xHandleLedsAction;

    // -------------------------------------------------------------------------
    // Private methods
    // -------------------------------------------------------------------------

    /** Apply a pattern to the LED display (replaces LEDS_ShowPattern) */
    void LEDS_ShowPattern(uint8_t pattern);

    /** Message handler task (replaces LEDS_HandlerThread) */
    void LEDS_HandlerThread(void);

    /** Periodic animation task (replaces LEDS_ActionThread) */
    void LEDS_ActionThread(void);

    /** Test task — do not use in normal operation (replaces LEDS_Tests) */
    void LEDS_Tests(void);

    // -------------------------------------------------------------------------
    // GPIO helpers (inline replacements for the original macros)
    // -------------------------------------------------------------------------

    inline void LEDS_All_Off(void) {
        HAL_GPIO_WritePin(GPIOB,
            LED_SEG_A_Pin | LED_SEG_B_Pin | LED_SEG_C_Pin,
            GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA,
            LED_SEG_D_Pin | LED_SEG_E_Pin | LED_SEG_F_Pin
            | LED_SEG_G_Pin | LED_SEG_DP_Pin,
            GPIO_PIN_RESET);
    }

    inline void LEDS_All_On(void) {
        HAL_GPIO_WritePin(GPIOB,
            LED_SEG_A_Pin | LED_SEG_B_Pin | LED_SEG_C_Pin,
            GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOA,
            LED_SEG_D_Pin | LED_SEG_E_Pin | LED_SEG_F_Pin
            | LED_SEG_G_Pin | LED_SEG_DP_Pin,
            GPIO_PIN_SET);
    }

    // -------------------------------------------------------------------------
    // FreeRTOS task trampolines
    // -------------------------------------------------------------------------

    static void LEDS_HandlerThread_trampoline(void* pvParameters);
    static void LEDS_ActionThread_trampoline(void* pvParameters);
    static void LEDS_Tests_trampoline(void* pvParameters);
};

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* LEDS_H */
