/*
 * commands.h
 *
 *  Created on: 18 mars 2026
 *      Author: Marine OUVRARS
 */

#ifndef COMMANDS_H_
#define COMMANDS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "application.h"
#include "messages.h"
#include <string.h>   // strlen, strncpy, memcpy
#include <stdlib.h>   // malloc, free, strtol
#include <limits.h>   // SHRT_MAX, SHRT_MIN

/**
 * @brief Classe pour gérer les commandes robot
 */
class Commands {
public:
    // === ENUMS ===
    enum CMD_CommandsType : uint8_t {
        CMD_NONE=0x0,
        CMD_PING,
        CMD_RESET,
        CMD_START_WITH_WATCHDOG,
        CMD_RESET_WATCHDOG,
        CMD_GET_BATTERY,
        CMD_GET_VERSION,
        CMD_START_WITHOUT_WATCHDOG,
        CMD_MOVE,
        CMD_TURN,
        CMD_GET_BUSY_STATE,
        CMD_TEST,
        CMD_DEBUG,
        CMD_POWER_OFF,
        CMD_INVALID_CHECKSUM=0xFF
    };

    enum CMD_AnswersType : uint8_t {
        ANS_OK=0x80,
        ANS_ERR,
        ANS_UNKNOWN
    };

    enum CMD_BatteryLevelType : uint8_t {
        ANS_BAT_EMPTY=0,
        ANS_BAT_LOW,
        ANS_BAT_OK
    };

    enum CMD_BusyType : uint8_t {
        ANS_STATE_NOT_BUSY=0,
        ANS_STATE_BUSY
    };

    // === STRUCTS ===
    struct CMD_Generic {
        uint8_t type;
    };

    struct CMD_Move : CMD_Generic {
        int16_t distance;
    };

    struct CMD_Turn : CMD_Generic {
        int16_t turns;
    };

    // === PUBLIC STATIC METHODS ===
    static CMD_Generic* decode(char* cmd, uint8_t length);
    static void sendAnswer(uint8_t ans);
    static void sendString(char* str);
    static void sendBatteryLevel(uint16_t batteryState);
    static void sendVersion();
    static void sendBusyState(uint8_t state);

private:
    // === PRIVATE STATIC METHODS ===
    static char* addChecksum(const char* str);
    static char verifyChecksum(char* str);

    // === COMMAND CHARACTERS ===
    static constexpr char PingCMD = 'p';
    static constexpr char ResetCMD = 'r';
    static constexpr char MoveCMD = 'M';
    static constexpr char TurnCMD = 'T';
    static constexpr char StartWWatchDogCMD = 'W';
    static constexpr char ResetWatchdogCMD = 'w';
    static constexpr char GetBatteryVoltageCMD = 'v';
    static constexpr char GetVersionCMD = 'V';
    static constexpr char StartWithoutWatchCMD = 'u';
    static constexpr char BusyStateCMD = 'b';
    static constexpr char TestCMD = 't';
    static constexpr char DebugCMD = 'a';
    static constexpr char PowerOffCMD = 'z';

    // === ANSWER STRINGS ===
    static constexpr const char* OK_ANS = "O\r";
    static constexpr const char* ERR_ANS = "E\r";
    static constexpr const char* UNKNOW_ANS = "C\r";
};

#ifdef __cplusplus
}
#endif

#endif /* COMMANDS_H_ */
