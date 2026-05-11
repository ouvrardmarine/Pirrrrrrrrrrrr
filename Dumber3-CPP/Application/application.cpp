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
#include "Motors.h"
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
	MESSAGE_Init();
//
    /* Init de l'afficheur */
    leds.LEDS_Init();
    Motors::init();
    xbee.XBEE_Init();

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

Leds& Application::GetLeds(void){
	return leds;
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
    MESSAGE_Typedef msg;
    char*        receivedCMD;
    Commands::CMD_Generic* decodedCmd;

    while (1) {
        msg = MESSAGE_ReadMailbox(APPLICATION_Mailbox);

        switch (msg.id) {
        case MSG_ID_XBEE_CMD:
            receivedCMD = (char*) msg.data;

            if (receivedCMD != NULL) {
                decodedCmd = Commands::decode(receivedCMD, strlen(receivedCMD));

                if (decodedCmd != NULL) {
                    if (decodedCmd->type == Commands::CMD_NONE) {
                    	Commands::sendAnswer(Commands::ANS_UNKNOWN);
                    } else if (decodedCmd->type == Commands::CMD_INVALID_CHECKSUM) {
                    	Commands::sendAnswer(Commands::ANS_ERR);
                    } else {
                        systemInfos.cmd = decodedCmd->type;
                        systemTimeout.inactivityCnt = 0;

                        /* Manage answer to command when possible.
                         * Further treatment is done in APPLICATION_StateMachine. */
                        switch (decodedCmd->type) {
                        case Commands::CMD_PING:
                        case Commands::CMD_TEST:
                        case Commands::CMD_DEBUG:
                        	Commands::sendAnswer(Commands::ANS_OK);
                            break;
                        case Commands::CMD_POWER_OFF:
                            systemInfos.powerOffRequired = 1;
                            Commands::sendAnswer(Commands::ANS_OK);
                            break;
                        case Commands::CMD_GET_BATTERY:
                        	Commands::sendBatteryLevel(systemInfos.batteryState);
                            break;
                        case Commands::CMD_GET_VERSION:
                        	Commands::sendVersion();
                            break;
                        case Commands::CMD_GET_BUSY_STATE:
                            if (systemInfos.state == stateInMouvement)
                            	Commands::sendBusyState(0x1);
                            else
                            	Commands::sendBusyState(0x0);
                            break;
                        case Commands::CMD_MOVE:
                            systemInfos.distance = ((Commands::CMD_Move*) decodedCmd)->distance;
                            break;
                        case Commands::CMD_TURN:
                            systemInfos.turns = ((Commands::CMD_Turn*) decodedCmd)->turns;
                            break;
                        default:
                            /* All other commands are processed in the state machine */
                            break;
                        }
                    }

                    free(receivedCMD);
                    free(decodedCmd);
                }
            }
            break;

        case MSG_ID_BAT_ADC_ERR:
        	Panic_Raise(PanicType::AdcError);
            break;

        case MSG_ID_BAT_CHARGE_ERR:
        	Panic_Raise(PanicType::ChargerError);
            break;

        case MSG_ID_BAT_CHARGE_COMPLETE:
        case MSG_ID_BAT_CHARGE_LOW:
        case MSG_ID_BAT_CHARGE_MED:
        case MSG_ID_BAT_CHARGE_HIGH:
            systemInfos.batteryUpdate = 1;
            systemInfos.inCharge      = 1;
            systemInfos.batteryState  = msg.id;
            break;

        case MSG_ID_BAT_CRITICAL_LOW:
        case MSG_ID_BAT_LOW:
        case MSG_ID_BAT_MED:
        case MSG_ID_BAT_HIGH:
            systemInfos.batteryUpdate = 1;
            systemInfos.inCharge      = 0;
            systemInfos.batteryState  = msg.id;
            break;

        case MSG_ID_MOTORS_END_OF_MOUVMENT:
            systemInfos.endOfMouvement = 1;
            break;

        case MSG_ID_BUTTON_PRESSED:
            systemInfos.powerOffRequired = 1;
            break;

        default:
            break;
        }

        APPLICATION_StateMachine();
    }
//	while (1) {
//		vTaskDelay(pdMS_TO_TICKS(1000));
//	}
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
	Leds::LEDS_State ledState = Leds::leds_off;

    if (systemInfos.powerOffRequired)
        APPLICATION_PowerOff(); /* system halts here */

    if ((systemInfos.inCharge) && (systemInfos.state != stateInCharge)) {
        APPLICATION_TransitionToNewState(stateInCharge);
    }

    if (systemInfos.batteryUpdate) {
        if (systemInfos.batteryState == MSG_ID_BAT_CRITICAL_LOW) {
        	ledState = Leds::leds_bat_critical_low;
            APPLICATION_TransitionToNewState(stateLowBatDisable);
            leds.LEDS_Set(ledState);
        } else if (systemInfos.state == stateInCharge) {
            switch (systemInfos.batteryState) {
            case MSG_ID_BAT_CHARGE_COMPLETE: ledState = Leds::leds_bat_charge_complete; break;
            case MSG_ID_BAT_CHARGE_HIGH:     ledState = Leds::leds_bat_charge_high;     break;
            case MSG_ID_BAT_CHARGE_MED:      ledState = Leds::leds_bat_charge_med;      break;
            case MSG_ID_BAT_CHARGE_LOW:      ledState = Leds::leds_bat_charge_low;      break;
            }
            leds.LEDS_Set(ledState);
        } else if (systemInfos.state == stateStartup) {
            switch (systemInfos.batteryState) {
            case MSG_ID_BAT_HIGH: ledState = Leds::leds_bat_high; break;
            case MSG_ID_BAT_MED:  ledState = Leds::leds_bat_med;  break;
            case MSG_ID_BAT_LOW:  ledState = Leds::leds_bat_low;  break;
            }
            leds.LEDS_Set(ledState);
        }
    }

    if (systemInfos.cmd != Commands::CMD_NONE) {
        switch (systemInfos.cmd) {
        case Commands::CMD_RESET:
            if ((systemInfos.state == stateIdle)         ||
                (systemInfos.state == stateRun)          ||
                (systemInfos.state == stateInMouvement)  ||
                (systemInfos.state == stateWatchdogDisable))
            {
            	Commands::sendAnswer(Commands::ANS_OK);
                APPLICATION_TransitionToNewState(stateIdle);
            } else {
            	Commands::sendAnswer(Commands::ANS_ERR);
            }
            break;

        case Commands::CMD_START_WITH_WATCHDOG:
        case Commands::CMD_START_WITHOUT_WATCHDOG:
            if (systemInfos.state == stateIdle) {
            	Commands::sendAnswer(Commands::ANS_OK);

                if (systemInfos.cmd == Commands::CMD_START_WITH_WATCHDOG) {
                    systemTimeout.watchdogEnabled  = 1;
                    systemTimeout.watchdogCnt      = 0;
                    systemTimeout.watchdogMissedCnt = 0;
                }

                APPLICATION_TransitionToNewState(stateRun);
            } else {
            	Commands::sendAnswer(Commands::ANS_ERR);
            }
            break;

        case Commands::CMD_RESET_WATCHDOG:
            if ((systemInfos.state == stateRun) || (systemInfos.state == stateInMouvement)) {
                if ((systemTimeout.watchdogEnabled == 0) ||
                    ((systemTimeout.watchdogCnt >= (APPLICATION_WATCHDOG_MIN / 100)) &&
                     (systemTimeout.watchdogCnt <= (APPLICATION_WATCHDOG_MAX / 100))))
                {
                    systemTimeout.watchdogMissedCnt = 0;
                    Commands::sendAnswer(Commands::ANS_OK);
                } else {
                    systemTimeout.watchdogMissedCnt++;
                    Commands::sendAnswer(Commands::ANS_ERR);
                }
                systemTimeout.watchdogCnt = 0;
            } else {
                Commands::sendAnswer(Commands::ANS_ERR);
            }
            break;

        case Commands::CMD_MOVE:
        case Commands::CMD_TURN:
            /* +++ evoxx-probleme-refresh-wdt-moteurs-on */
            if ((systemInfos.state == stateRun) || (systemInfos.state == stateInMouvement)) {
                if (((systemInfos.cmd == Commands::CMD_MOVE) && (systemInfos.distance != 0)) ||
                    ((systemInfos.cmd == Commands::CMD_TURN) && (systemInfos.turns != 0)))
                {
                    systemInfos.endOfMouvement = 0;
                    APPLICATION_TransitionToNewState(stateInMouvement);
                } else {
                    if (((systemInfos.cmd == Commands::CMD_MOVE) && (systemInfos.distance == 0)) ||
                        ((systemInfos.cmd == Commands::CMD_TURN) && (systemInfos.turns == 0)))
                    {
                        systemInfos.endOfMouvement = 1;
                    }
                }
                Commands::sendAnswer(Commands::ANS_OK);
            } else {
                Commands::sendAnswer(Commands::ANS_ERR);
            }
            /* --- evoxx-probleme-refresh-wdt-moteurs-on */
            break;

        default:
            break;
        }
    }

    if ((systemInfos.state == stateInMouvement) && (systemInfos.endOfMouvement)) {
        APPLICATION_TransitionToNewState(stateRun);
    }

    if (systemInfos.state == stateInCharge) {
        if (!systemInfos.inCharge) {
            APPLICATION_TransitionToNewState(stateIdle);
        } else if (systemInfos.batteryUpdate) {
            APPLICATION_TransitionToNewState(stateInCharge);
        }
    }

    systemInfos.batteryUpdate    = 0;
    systemInfos.cmd              = Commands::CMD_NONE;
    systemInfos.endOfMouvement   = 0;
    systemInfos.powerOffRequired = 0;
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
	Leds::LEDS_State ledState = Leds::leds_off;

    switch (new_state) {
    case stateStartup:
       /* nothing to do here */
        break;

    case stateIdle:
        ledState = Leds::leds_idle;
        leds.LEDS_Set(ledState);
        Motors::stop();
        systemTimeout.inactivityCnt  = 0;
        systemTimeout.watchdogEnabled = 0;
        break;

    case stateRun:
        ledState = systemTimeout.watchdogEnabled ? Leds::leds_run_with_watchdog : Leds::leds_run;
        leds.LEDS_Set(ledState);
        Motors::stop();
        break;

    case stateInMouvement:
        /* +++ evoxx-bug-watchdog-avec-moteurs */
        ledState = systemTimeout.watchdogEnabled ? Leds::leds_run_with_watchdog : Leds::leds_run;
        leds.LEDS_Set(ledState);

        if (systemInfos.cmd == Commands::CMD_MOVE)
        	Motors::move(systemInfos.distance);
        else
        	Motors::move(systemInfos.turns);
        break;

    case stateInCharge:
        /* LEDs are managed in APPLICATION_StateMachine */
    	Motors::stop();
        systemTimeout.watchdogEnabled = 0;
        break;

    case stateWatchdogDisable:
        ledState = Leds::leds_watchdog_expired;
        leds.LEDS_Set(ledState);
        Motors::stop(); /* +++ evoxx-bug-watchdog-avec-moteurs */
        systemTimeout.watchdogEnabled = 0;
        break;

    case stateLowBatDisable:
        ledState = Leds::leds_bat_critical_low;
        leds.LEDS_Set(ledState);
        Motors::stop(); /* +++ evoxx-bug-watchdog-avec-moteurs */
        systemTimeout.watchdogEnabled = 0;

        /* Send Button_Pressed as priority message to trigger power-off.
         * Done before the delay so the mailbox does not fill up. */
        MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BUTTON_PRESSED,
                APPLICATION_Mailbox, (void*) NULL);

        vTaskDelay(pdMS_TO_TICKS(4000)); /* wait 4 s */
        break;

    default:
        break;
    }

    systemInfos.state = new_state;
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
    if (systemInfos.state == stateStartup) {
        systemTimeout.startupCnt++;
        if (systemTimeout.startupCnt++ >= (APPLICATION_STARTUP_DELAY / 100))
            APPLICATION_TransitionToNewState(stateIdle);
    }

    if (systemInfos.state != stateInCharge) {
        systemTimeout.inactivityCnt++;
        if (systemTimeout.inactivityCnt >= (APPLICATION_INACTIVITY_TIMEOUT / 100))
            /* Send Button_Pressed to trigger power-off */
            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_BUTTON_PRESSED,
                    APPLICATION_Mailbox, (void*) NULL);

        if (systemTimeout.watchdogEnabled) {
            systemTimeout.watchdogCnt++;

            if (systemTimeout.watchdogCnt > (APPLICATION_WATCHDOG_MAX / 100)) {
                systemTimeout.watchdogCnt = 0;
                systemTimeout.watchdogMissedCnt++;
            }

            if (systemTimeout.watchdogMissedCnt >= APPLICATION_WATCHDOG_MISSED_MAX)
                APPLICATION_TransitionToNewState(stateWatchdogDisable);
        }
    }
}

/**
 * @}
 */

/**
 * @}
 */
