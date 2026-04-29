/**
 ******************************************************************************
 * @file application.cpp
 * @brief application body (C++ version)
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

#include "application.h"

#include "xbee.h"
#include "messages.h"
#include "commands.h"
#include "moteur.h"
#include "battery_version4.hpp"
#include "panic_version4.hpp"
#include "rtos_support.h"

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup APPLICATION
 * @{
 */

// =========================================================================
// Constructor
// =========================================================================

Application::Application()
    : xHandleApplicationMain(NULL),
      xHandleTimerTimeout(NULL)
{
    memset(&systemInfos,   0, sizeof(systemInfos));
    memset(&systemTimeout, 0, sizeof(systemTimeout));
}

// =========================================================================
// FreeRTOS trampolines
// =========================================================================

void Application::APPLICATION_Thread_trampoline(void* pvParameters) {
    static_cast<Application*>(pvParameters)->APPLICATION_Thread();
}

/**
 * @brief Static trampoline for the FreeRTOS software timer callback.
 *
 * FreeRTOS timer callbacks receive only a TimerHandle_t. The instance pointer
 * is stored in the timer ID via pvTimerGetTimerID().
 */
void Application::vTimerTimeoutCallback_trampoline(TimerHandle_t xTimer) {
    Application* self = static_cast<Application*>(pvTimerGetTimerID(xTimer));
    self->vTimerTimeoutCallback(xTimer);
}

// =========================================================================
// Public API
// =========================================================================

/**
 * @brief  Initialization of drivers, modules and application.
 * @param  None
 * @return None
 */
void Application::Init(void) {
//    /* Init des messages box */
//    MESSAGE_Init();
//
    /* Init de l'afficheur */
    leds.LEDS_Init();

//    /* Init de la partie RF / reception des messages */
//    XBEE_Init();
//    BATTERY_Init();
//    MOTORS_Init();
//
    /* Create the task without using any dynamic memory allocation. */
    xHandleApplicationMain = xTaskCreateStatic(
            APPLICATION_Thread_trampoline, /* Function that implements the task. */
            "APPLICATION Thread",          /* Text name for the task. */
            STACK_SIZE * 2,               /* Number of indexes in the xStack array. */
            this,                          /* Parameter passed into the task (pointer to instance). */
            PriorityApplicationHandler,   /* Priority at which the task is created. */
            xStackApplicationMain,        /* Array to use as the task's stack. */
            &xTaskApplicationMain);       /* Variable to hold the task's data structure. */
    vTaskResume(xHandleApplicationMain);

    /* Create a periodic timer without using any dynamic memory allocation.
     * The instance pointer is stored as the timer ID so the trampoline can
     * recover it.  */
    xHandleTimerTimeout = xTimerCreateStatic(
            "Counters Timer",
            pdMS_TO_TICKS(APPLICATION_COUNTERS_DELAY),
            pdTRUE,                               /* auto-reload */
            static_cast<void*>(this),             /* timer ID = instance pointer */
            vTimerTimeoutCallback_trampoline,
            &xBufferTimerTimeout);
    xTimerStart(xHandleTimerTimeout, 0);
}

// =========================================================================
// Private methods
// =========================================================================

/**
 * @brief  Application thread (main thread)
 *
 * Waits for messages from other threads or drivers, stores information, sets
 * various flags and then calls APPLICATION_StateMachine() for processing.
 *
 * @return None
 */
void Application::APPLICATION_Thread(void) {
//    MESSAGE_Typedef msg;
//    char*        receivedCMD;
//    CMD_Generic* decodedCmd;
//
//    while (1) {
//        msg = MESSAGE_ReadMailbox(APPLICATION_Mailbox);
//
//        switch (msg.id) {
//        case MSG_ID_XBEE_CMD:
//            receivedCMD = (char*) msg.data;
//
//            if (receivedCMD != NULL) {
//                decodedCmd = cmdDecode(receivedCMD, strlen(receivedCMD));
//
//                if (decodedCmd != NULL) {
//                    if (decodedCmd->type == CMD_NONE) {
//                        cmdSendAnswer(ANS_UNKNOWN);
//                    } else if (decodedCmd->type == CMD_INVALID_CHECKSUM) {
//                        cmdSendAnswer(ANS_ERR);
//                    } else {
//                        systemInfos.cmd = decodedCmd->type;
//                        systemTimeout.inactivityCnt = 0;
//
//                        /* Manage answer to command when possible.
//                         * Further treatment is done in APPLICATION_StateMachine. */
//                        switch (decodedCmd->type) {
//                        case CMD_PING:
//                        case CMD_TEST:
//                        case CMD_DEBUG:
//                            cmdSendAnswer(ANS_OK);
//                            break;
//                        case CMD_POWER_OFF:
//                            systemInfos.powerOffRequired = 1;
//                            cmdSendAnswer(ANS_OK);
//                            break;
//                        case CMD_GET_BATTERY:
//                            cmdSendBatteryLevel(systemInfos.batteryState);
//                            break;
//                        case CMD_GET_VERSION:
//                            cmdSendVersion();
//                            break;
//                        case CMD_GET_BUSY_STATE:
//                            if (systemInfos.state == stateInMouvement)
//                                cmdSendBusyState(0x1);
//                            else
//                                cmdSendBusyState(0x0);
//                            break;
//                        case CMD_MOVE:
//                            systemInfos.distance = ((CMD_Move*) decodedCmd)->distance;
//                            break;
//                        case CMD_TURN:
//                            systemInfos.turns = ((CMD_Turn*) decodedCmd)->turns;
//                            break;
//                        default:
//                            /* All other commands are processed in the state machine */
//                            break;
//                        }
//                    }
//
//                    free(receivedCMD);
//                    free(decodedCmd);
//                }
//            }
//            break;
//
//        case MSG_ID_BAT_ADC_ERR:
//            PANIC_Raise(panic_adc_err);
//            break;
//
//        case MSG_ID_BAT_CHARGE_ERR:
//            PANIC_Raise(panic_charger_err);
//            break;
//
//        case MSG_ID_BAT_CHARGE_COMPLETE:
//        case MSG_ID_BAT_CHARGE_LOW:
//        case MSG_ID_BAT_CHARGE_MED:
//        case MSG_ID_BAT_CHARGE_HIGH:
//            systemInfos.batteryUpdate = 1;
//            systemInfos.inCharge      = 1;
//            systemInfos.batteryState  = msg.id;
//            break;
//
//        case MSG_ID_BAT_CRITICAL_LOW:
//        case MSG_ID_BAT_LOW:
//        case MSG_ID_BAT_MED:
//        case MSG_ID_BAT_HIGH:
//            systemInfos.batteryUpdate = 1;
//            systemInfos.inCharge      = 0;
//            systemInfos.batteryState  = msg.id;
//            break;
//
//        case MSG_ID_MOTORS_END_OF_MOUVMENT:
//            systemInfos.endOfMouvement = 1;
//            break;
//
//        case MSG_ID_BUTTON_PRESSED:
//            systemInfos.powerOffRequired = 1;
//            break;
//
//        default:
//            break;
//        }
//
//        APPLICATION_StateMachine();
//    }
}

/**
 * @brief  State machine processing function
 *
 * Processes received messages depending on the current system state.
 * When a state transition is needed, APPLICATION_TransitionToNewState is called.
 *
 * @param  None
 * @return None
 */
void Application::APPLICATION_StateMachine(void) {
//    LEDS_State ledState = leds_off;
//
//    if (systemInfos.powerOffRequired)
//        APPLICATION_PowerOff(); /* system halts here */
//
//    if ((systemInfos.inCharge) && (systemInfos.state != stateInCharge)) {
//        APPLICATION_TransitionToNewState(stateInCharge);
//    }
//
//    if (systemInfos.batteryUpdate) {
//        if (systemInfos.batteryState == MSG_ID_BAT_CRITICAL_LOW) {
//            ledState = leds_bat_critical_low;
//            APPLICATION_TransitionToNewState(stateLowBatDisable);
//            LEDS_Set(ledState);
//        } else if (systemInfos.state == stateInCharge) {
//            switch (systemInfos.batteryState) {
//            case MSG_ID_BAT_CHARGE_COMPLETE: ledState = leds_bat_charge_complete; break;
//            case MSG_ID_BAT_CHARGE_HIGH:     ledState = leds_bat_charge_high;     break;
//            case MSG_ID_BAT_CHARGE_MED:      ledState = leds_bat_charge_med;      break;
//            case MSG_ID_BAT_CHARGE_LOW:      ledState = leds_bat_charge_low;      break;
//            }
//            LEDS_Set(ledState);
//        } else if (systemInfos.state == stateStartup) {
//            switch (systemInfos.batteryState) {
//            case MSG_ID_BAT_HIGH: ledState = leds_bat_high; break;
//            case MSG_ID_BAT_MED:  ledState = leds_bat_med;  break;
//            case MSG_ID_BAT_LOW:  ledState = leds_bat_low;  break;
//            }
//            LEDS_Set(ledState);
//        }
//    }
//
//    if (systemInfos.cmd != CMD_NONE) {
//        switch (systemInfos.cmd) {
//        case CMD_RESET:
//            if ((systemInfos.state == stateIdle)         ||
//                (systemInfos.state == stateRun)          ||
//                (systemInfos.state == stateInMouvement)  ||
//                (systemInfos.state == stateWatchdogDisable))
//            {
//                cmdSendAnswer(ANS_OK);
//                APPLICATION_TransitionToNewState(stateIdle);
//            } else {
//                cmdSendAnswer(ANS_ERR);
//            }
//            break;
//
//        case CMD_START_WITH_WATCHDOG:
//        case CMD_START_WITHOUT_WATCHDOG:
//            if (systemInfos.state == stateIdle) {
//                cmdSendAnswer(ANS_OK);
//
//                if (systemInfos.cmd == CMD_START_WITH_WATCHDOG) {
//                    systemTimeout.watchdogEnabled  = 1;
//                    systemTimeout.watchdogCnt      = 0;
//                    systemTimeout.watchdogMissedCnt = 0;
//                }
//
//                APPLICATION_TransitionToNewState(stateRun);
//            } else {
//                cmdSendAnswer(ANS_ERR);
//            }
//            break;
//
//        case CMD_RESET_WATCHDOG:
//            if ((systemInfos.state == stateRun) || (systemInfos.state == stateInMouvement)) {
//                if ((systemTimeout.watchdogEnabled == 0) ||
//                    ((systemTimeout.watchdogCnt >= (APPLICATION_WATCHDOG_MIN / 100)) &&
//                     (systemTimeout.watchdogCnt <= (APPLICATION_WATCHDOG_MAX / 100))))
//                {
//                    systemTimeout.watchdogMissedCnt = 0;
//                    cmdSendAnswer(ANS_OK);
//                } else {
//                    systemTimeout.watchdogMissedCnt++;
//                    cmdSendAnswer(ANS_ERR);
//                }
//                systemTimeout.watchdogCnt = 0;
//            } else {
//                cmdSendAnswer(ANS_ERR);
//            }
//            break;
//
//        case CMD_MOVE:
//        case CMD_TURN:
//            /* +++ evoxx-probleme-refresh-wdt-moteurs-on */
//            if ((systemInfos.state == stateRun) || (systemInfos.state == stateInMouvement)) {
//                if (((systemInfos.cmd == CMD_MOVE) && (systemInfos.distance != 0)) ||
//                    ((systemInfos.cmd == CMD_TURN) && (systemInfos.turns    != 0)))
//                {
//                    systemInfos.endOfMouvement = 0;
//                    APPLICATION_TransitionToNewState(stateInMouvement);
//                } else {
//                    if (((systemInfos.cmd == CMD_MOVE) && (systemInfos.distance == 0)) ||
//                        ((systemInfos.cmd == CMD_TURN) && (systemInfos.turns    == 0)))
//                    {
//                        systemInfos.endOfMouvement = 1;
//                    }
//                }
//                cmdSendAnswer(ANS_OK);
//            } else {
//                cmdSendAnswer(ANS_ERR);
//            }
//            /* --- evoxx-probleme-refresh-wdt-moteurs-on */
//            break;
//
//        default:
//            break;
//        }
//    }
//
//    if ((systemInfos.state == stateInMouvement) && (systemInfos.endOfMouvement)) {
//        APPLICATION_TransitionToNewState(stateRun);
//    }
//
//    if (systemInfos.state == stateInCharge) {
//        if (!systemInfos.inCharge) {
//            APPLICATION_TransitionToNewState(stateIdle);
//        } else if (systemInfos.batteryUpdate) {
//            APPLICATION_TransitionToNewState(stateInCharge);
//        }
//    }
//
//    systemInfos.batteryUpdate    = 0;
//    systemInfos.cmd              = CMD_NONE;
//    systemInfos.endOfMouvement   = 0;
//    systemInfos.powerOffRequired = 0;
}

/**
 * @brief  State machine transition and cleanup
 *
 * Processes and cleans up state machine transitions.
 *
 * @param[in] new_state New state to apply to the system
 * @return None
 */
void Application::APPLICATION_TransitionToNewState(APPLICATION_State new_state) {
//    LEDS_State ledState = leds_off;
//
//    switch (new_state) {
//    case stateStartup:
//        /* nothing to do here */
//        break;
//
//    case stateIdle:
//        ledState = leds_idle;
//        LEDS_Set(ledState);
//        MOTORS_Stop();
//        systemTimeout.inactivityCnt  = 0;
//        systemTimeout.watchdogEnabled = 0;
//        break;
//
//    case stateRun:
//        ledState = systemTimeout.watchdogEnabled ? leds_run_with_watchdog : leds_run;
//        LEDS_Set(ledState);
//        MOTORS_Stop();
//        break;
//
//    case stateInMouvement:
//        /* +++ evoxx-bug-watchdog-avec-moteurs */
//        ledState = systemTimeout.watchdogEnabled ? leds_run_with_watchdog : leds_run;
//        LEDS_Set(ledState);
//
//        if (systemInfos.cmd == CMD_MOVE)
//            MOTORS_Move(systemInfos.distance);
//        else
//            MOTORS_Turn(systemInfos.turns);
//        break;
//
//    case stateInCharge:
//        /* LEDs are managed in APPLICATION_StateMachine */
//        MOTORS_Stop();
//        systemTimeout.watchdogEnabled = 0;
//        break;
//
//    case stateWatchdogDisable:
//        ledState = leds_watchdog_expired;
//        LEDS_Set(ledState);
//        MOTORS_Stop(); /* +++ evoxx-bug-watchdog-avec-moteurs */
//        systemTimeout.watchdogEnabled = 0;
//        break;
//
//    case stateLowBatDisable:
//        ledState = leds_bat_critical_low;
//        LEDS_Set(ledState);
//        MOTORS_Stop(); /* +++ evoxx-bug-watchdog-avec-moteurs */
//        systemTimeout.watchdogEnabled = 0;
//
//        /* Send Button_Pressed as priority message to trigger power-off.
//         * Done before the delay so the mailbox does not fill up. */
//        MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BUTTON_PRESSED,
//                APPLICATION_Mailbox, (void*) NULL);
//
//        vTaskDelay(pdMS_TO_TICKS(4000)); /* wait 4 s */
//        break;
//
//    default:
//        break;
//    }
//
//    systemInfos.state = new_state;
}

/**
 * @brief  Power off robot
 *
 * Disables the main regulator and powers off the system.
 * Called after inactivity or when the user presses the on/off button.
 *
 * @param  None
 * @return None
 */
void Application::APPLICATION_PowerOff(void) {
    HAL_GPIO_WritePin(SHUTDOWN_GPIO_Port, SHUTDOWN_Pin, GPIO_PIN_RESET);

    while (1) {
        __WFE(); /* Wait forever for the regulator to cut power */
    }
}

/**
 * @brief  Periodic callback for system counter update (called every 100 ms)
 *
 * Updates inactivity, startup and watchdog counters.
 * Sends messages or triggers state transitions as necessary.
 *
 * @remark Time constants are in ms, hence the division by 100 in comparisons.
 *
 * @param[in] xTimer Handler for the periodic timer
 * @return None
 */
void Application::vTimerTimeoutCallback(TimerHandle_t xTimer) {
//    if (systemInfos.state == stateStartup) {
//        systemTimeout.startupCnt++;
//        if (systemTimeout.startupCnt++ >= (APPLICATION_STARTUP_DELAY / 100))
//            APPLICATION_TransitionToNewState(stateIdle);
//    }
//
//    if (systemInfos.state != stateInCharge) {
//        systemTimeout.inactivityCnt++;
//        if (systemTimeout.inactivityCnt >= (APPLICATION_INACTIVITY_TIMEOUT / 100))
//            /* Send Button_Pressed to trigger power-off */
//            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BUTTON_PRESSED,
//                    APPLICATION_Mailbox, (void*) NULL);
//
//        if (systemTimeout.watchdogEnabled) {
//            systemTimeout.watchdogCnt++;
//
//            if (systemTimeout.watchdogCnt > (APPLICATION_WATCHDOG_MAX / 100)) {
//                systemTimeout.watchdogCnt = 0;
//                systemTimeout.watchdogMissedCnt++;
//            }
//
//            if (systemTimeout.watchdogMissedCnt >= APPLICATION_WATCHDOG_MISSED_MAX)
//                APPLICATION_TransitionToNewState(stateWatchdogDisable);
//        }
//    }
}

/**
 * @}
 */

/**
 * @}
 */
