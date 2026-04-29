/*
 * commands.cpp
 *
 *  Created on: 18 mars 2026
 *      Author: Marine OUVRARD
 */

#include "Commands.h"

// === PRIVATE METHODS ===
char* Commands::addChecksum(const char* str) {
    size_t len = strlen(str);
    char* outstr = new char[len + 3]; // +checksum + \r + \0
    unsigned char checksum = 0;

    for (size_t j = 0; str[j] != '\r'; j++)
        checksum ^= str[j];

    if (checksum == '\r') checksum++;

    memcpy(outstr, str, len);
    outstr[len] = checksum;
    outstr[len + 1] = '\r';
    outstr[len + 2] = 0;

    return outstr;
}

char Commands::verifyChecksum(char* str) {
    size_t len = strlen(str);
    if (len == 0) return 0;

    unsigned char checksum = 0;
    for (size_t j = 0; j < len - 1; j++)
        checksum ^= str[j];

    if (checksum == '\r') checksum++;

    if (str[len - 1] == checksum) {
        str[len - 1] = 0;
        str[len] = 0;
        return 1;
    }
    return 0;
}

// === PUBLIC METHODS ===
Commands::CMD_Generic* Commands::decode(char* cmd, uint8_t length) {
    CMD_Generic* decodedCmd = nullptr;
    char cmd_type = cmd[0];
    char* p;
    long value = 0;

    if (!verifyChecksum(cmd)) {
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_INVALID_CHECKSUM;
        return decodedCmd;
    }

    switch (cmd_type) {
    case MoveCMD:
        decodedCmd = new CMD_Move();
        decodedCmd->type = CMD_MOVE;
        if ((cmd[0]=='M')&&(cmd[1]=='=')) {
            cmd += 2;
            value = strtol(cmd, &p, 10);
            if (p == cmd) decodedCmd->type = CMD_NONE;
            else {
                if (value > SHRT_MAX) value = SHRT_MAX;
                if (value < SHRT_MIN) value = SHRT_MIN;
                static_cast<CMD_Move*>(decodedCmd)->distance = (int16_t)value;
            }
        } else decodedCmd->type = CMD_NONE;
        break;

    case TurnCMD:
        decodedCmd = new CMD_Turn();
        decodedCmd->type = CMD_TURN;
        if ((cmd[0]=='T')&&(cmd[1]=='=')) {
            cmd += 2;
            value = strtol(cmd, &p, 10);
            if (p == cmd) decodedCmd->type = CMD_NONE;
            else {
                if (value > SHRT_MAX) value = SHRT_MAX;
                if (value < SHRT_MIN) value = SHRT_MIN;
                static_cast<CMD_Turn*>(decodedCmd)->turns = (int16_t)value;
            }
        } else decodedCmd->type = CMD_NONE;
        break;

    case PingCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_PING;
        break;

    case ResetCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_RESET;
        break;

    case StartWWatchDogCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_START_WITH_WATCHDOG;
        break;

    case StartWithoutWatchCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_START_WITHOUT_WATCHDOG;
        break;

    case ResetWatchdogCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_RESET_WATCHDOG;
        break;

    case GetBatteryVoltageCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_GET_BATTERY;
        break;

    case GetVersionCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_GET_VERSION;
        break;

    case BusyStateCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_GET_BUSY_STATE;
        break;

    case TestCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_TEST;
        break;

    case DebugCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_DEBUG;
        break;

    case PowerOffCMD:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_POWER_OFF;
        break;

    default:
        decodedCmd = new CMD_Generic();
        decodedCmd->type = CMD_NONE;
    }

    return decodedCmd;
}

void Commands::sendAnswer(uint8_t ans) {
    const char* str;
    switch (ans) {
    case ANS_OK: str = OK_ANS; break;
    case ANS_ERR: str = ERR_ANS; break;
    default: str = UNKNOW_ANS; break;
    }

    char* answer = addChecksum(str);
    MESSAGE_SendMailbox(XBEE_Mailbox, MSG_ID_XBEE_ANS, APPLICATION_Mailbox, answer);
}

void Commands::sendString(char* str) {
    int len = strlen(str);
    char local[len + 2];
    strncpy(local, str, len + 2);
    if (local[len - 1] != '\r') {
        local[len] = '\r';
        local[len + 1] = '\0';
    }
    char* answer = addChecksum(local);
    MESSAGE_SendMailbox(XBEE_Mailbox, MSG_ID_XBEE_ANS, APPLICATION_Mailbox, answer);
}

void Commands::sendBatteryLevel(uint16_t batteryState) {
    const char* str;
    switch (batteryState) {
    case MSG_ID_BAT_CHARGE_COMPLETE:
    case MSG_ID_BAT_CHARGE_HIGH:
    case MSG_ID_BAT_HIGH:
        str = "2\r"; break;
    case MSG_ID_BAT_CHARGE_MED:
    case MSG_ID_BAT_MED:
        str = "1\r"; break;
    default:
        str = "0\r"; break;
    }
    char* answer = addChecksum(str);
    MESSAGE_SendMailbox(XBEE_Mailbox, MSG_ID_XBEE_ANS, APPLICATION_Mailbox, answer);
}

void Commands::sendVersion() {
    int len = strlen(SYSTEM_VERSION_STR);
    char version[len + 2];
    strncpy(version, SYSTEM_VERSION_STR, len + 2);
    version[len] = '\r';
    version[len + 1] = '\0';
    char* answer = addChecksum(version);
    MESSAGE_SendMailbox(XBEE_Mailbox, MSG_ID_XBEE_ANS, APPLICATION_Mailbox, answer);
}

void Commands::sendBusyState(uint8_t state) {
    const char* str = state ? "1\r" : "0\r";
    char* answer = addChecksum(str);
    MESSAGE_SendMailbox(XBEE_Mailbox, MSG_ID_XBEE_ANS, APPLICATION_Mailbox, answer);
}
