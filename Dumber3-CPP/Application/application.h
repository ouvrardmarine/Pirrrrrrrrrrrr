/*
 * application.h
 *
 *  Created on: 18 mars 2026
 *      Author: 33783
 */

#ifndef APPLICATION_H_
#define APPLICATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "config.h"

#include "string.h"
#include <stdlib.h>

#include "leds.h"
#include "xbee.h"
#include "Motors.h"

#include "FreeRTOS.h"
#include "timers.h"

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup APPLICATION
 * @{
 */

/**
 * @brief Class encapsulating the main application logic.
 *
 * Manages the system state machine, periodic timeouts, driver initialisation,
 * and message dispatching for the Dumber robot.
 */
class Application {
public:
    // -------------------------------------------------------------------------
    // Public API — same interface as the original C functions
    // -------------------------------------------------------------------------

    /**
     * @brief Constructor: zero-initialises internal state
     */
    Application();

    /**
     * Singleton: instanciation unique de l'objet
     */
    static Application& Instance() {
           static Application instance;
           return instance;
       }

    /**
     * @brief Initialization of drivers, modules and application (replaces APPLICATION_Init)
     */
    void Init(void);

    Leds& GetLeds(void);

    void suspend() {
        if (xHandleApplicationMain) vTaskSuspend(xHandleApplicationMain);
    }

private:
    // -------------------------------------------------------------------------
    // Internal types
    // -------------------------------------------------------------------------

    /** Application state machine states */
    typedef enum {
        stateStartup = 0,       /**< Startup state, after system power on */
        stateIdle,              /**< Idle state, ready to handle commands */
        stateRun,               /**< Run state, after StartWithWatchdog/StartWithoutWatchdog */
        stateInCharge,          /**< In Charge state, charger plugged */
        stateInMouvement,       /**< In Movement state, robot is moving */
        stateWatchdogDisable,   /**< Watchdog Disable state, watchdog expired */
        stateLowBatDisable      /**< Low Bat Disable state, battery too low */
    } APPLICATION_State;

    /** Current system information */
    typedef struct {
        APPLICATION_State state;    /**< Current application state */
        uint8_t  cmd;               /**< Current received command (CMD_NONE if none) */
        uint16_t batteryState;      /**< Last battery message from battery driver */
        char     batteryUpdate;     /**< Battery state changed and needs processing */
        char     inCharge;          /**< Robot is plugged for charging */
        int32_t  distance;          /**< Distance requested with MOVE command */
        int32_t  turns;             /**< Number of turns requested with TURN command */
        int32_t  motor_left;        /**< Speed for left motor */
        int32_t  motor_right;       /**< Speed for right motor */
        char     endOfMouvement;    /**< Last movement request has ended */
        char     powerOffRequired;  /**< System power off requested */
        uint16_t senderAddress;     /**< XBee sender address (not used) */
        uint8_t  rfProblem;         /**< XBee RF quality (not used) */
    } APPLICATION_Infos;

    /** Counters for watchdog and inactivity management */
    typedef struct {
        uint32_t startupCnt;        /**< Startup counter (battery animation delay) */
        uint32_t inactivityCnt;     /**< Inactivity counter (no command received) */
        uint32_t watchdogCnt;       /**< Watchdog counter, reset on RESET_WATCHDOG command */
        char     watchdogEnabled;   /**< Watchdog is active */
        char     watchdogMissedCnt; /**< Number of missed watchdog resets */
    } APPLICATION_Timeout;

    // -------------------------------------------------------------------------
    // FreeRTOS main task
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskApplicationMain;
    StackType_t   xStackApplicationMain[STACK_SIZE * 2];
    TaskHandle_t  xHandleApplicationMain;

    // -------------------------------------------------------------------------
    // FreeRTOS periodic timer
    // -------------------------------------------------------------------------

    StaticTimer_t  xBufferTimerTimeout;
    TimerHandle_t  xHandleTimerTimeout;

    // -------------------------------------------------------------------------
    // System state
    // -------------------------------------------------------------------------

    APPLICATION_Infos   systemInfos;
    APPLICATION_Timeout systemTimeout;

    // Attributes
    Leds leds;
    Xbee xbee;

    // -------------------------------------------------------------------------
    // Private methods
    // -------------------------------------------------------------------------

    /** Main application thread (replaces APPLICATION_Thread) */
    void APPLICATION_Thread(void);

    /** State machine processing (replaces APPLICATION_StateMachine) */
    void APPLICATION_StateMachine(void);

    /** Power off the robot (replaces APPLICATION_PowerOff) */
    void APPLICATION_PowerOff(void);

    /** State machine transition and cleanup (replaces APPLICATION_TransitionToNewState) */
    void APPLICATION_TransitionToNewState(APPLICATION_State new_state);

    /** Periodic timeout callback (replaces vTimerTimeoutCallback) */
    void vTimerTimeoutCallback(TimerHandle_t xTimer);

    // -------------------------------------------------------------------------
    // FreeRTOS trampolines
    // -------------------------------------------------------------------------

    static void APPLICATION_Thread_trampoline(void* pvParameters);
    static void vTimerTimeoutCallback_trampoline(TimerHandle_t xTimer);
};

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* APPLICATION_H */


