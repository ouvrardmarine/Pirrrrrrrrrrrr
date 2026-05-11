/**
 ******************************************************************************
 * @file xbee.h
 * @brief xbee driver header (C++ version)
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

#ifndef XBEE_H
#define XBEE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include "stm32l0xx_ll_usart.h"
#include "panic_version4.hpp"
#include <stdio.h>
#include "main.h"

/** @addtogroup Application_Software
 * @{
 */

/** @addtogroup XBEE
 * Xbee driver handles RF communications with supervisor
 *
 * @warning Very important information: robot uses a 2.5V power supply so
 *          XBEE MODULES FROM S1 GENERATION DON'T WORK.
 *          Use, at least, modules from S2 generation.
 * @{
 */

/**
 * @brief Class encapsulating the XBee RF communication driver.
 *
 * Manages UART-based RF communication with the supervisor,
 * including TX/RX tasks, interrupt handlers, and framing logic.
 */

#define XBEE_OK          0
#define XBEE_TX_ERROR   -1
#define XBEE_TX_TIMEOUT -2

class Xbee {
public:
    // -------------------------------------------------------------------------
    // Public API — same interface as the original C functions
    // -------------------------------------------------------------------------

    /**
     * @brief Constructor: zero-initialises internal state
     */
    Xbee();

    /**
     * @brief  Initialize the XBee system (replaces XBEE_Init)
     */
    void XBEE_Init(void);

    /**
     * @brief Transmission interrupt handler (replaces XBEE_TX_IRQHandler)
     *
     * Called when the USART transmit register is empty and ready for a new byte.
     */
    void XBEE_TX_IRQHandler(void);

    /**
     * @brief Reception interrupt handler (replaces XBEE_RX_IRQHandler)
     *
     * Called when the USART receive register is full with a newly received byte.
     */
    void XBEE_RX_IRQHandler(void);

    /**
     * @brief UART IRQ dispatcher (replaces LPUART1_IRQHandler)
     *
     * Dispatches the IRQ to XBEE_TX_IRQHandler, XBEE_RX_IRQHandler,
     * or clears error flags as appropriate.
     */
    void LPUART1_IRQHandler(void);

    void suspend(void);

private:
    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------

    /**
     * @anchor xbee_api2_escape_chars
     * @name XBEE API2 Escape characters
     */
    ///@{
    static const uint8_t XBEE_API_ESCAPE_CHAR    = 0x7D;
    static const uint8_t XBEE_API_START_OF_FRAME = 0x7E;
    static const uint8_t XBEE_API_XON            = 0x11;
    static const uint8_t XBEE_API_XOFF           = 0x13;
    ///@}

    /** Ending character used in the protocol between supervisor and robot */
    static const char    XBEE_ENDING_CHAR = '\r';

    static const uint8_t XBEE_RX_PHASE_SOF    = 0;
    static const uint8_t XBEE_RX_PHASE_HEADER = 1;
    static const uint8_t XBEE_RX_PHASE_BODY   = 2;

    // -------------------------------------------------------------------------
    // TX state
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskXbeeTXHandler;
    StackType_t   xStackXbeeTXHandler[STACK_SIZE];
    TaskHandle_t  xHandleXbeeTXHandler;

    SemaphoreHandle_t  xHandleSemaphoreTX;
    StaticSemaphore_t  xSemaphoreTX;

    uint8_t  txBuffer[XBEE_TX_BUFFER_MAX_LENGTH];
    uint16_t txIndex;
    uint16_t txRemainingData;
    uint16_t txDataToSend;

    // -------------------------------------------------------------------------
    // RX state
    // -------------------------------------------------------------------------

    StaticTask_t  xTaskXbeeRX;
    StackType_t   xStackXbeeRX[STACK_SIZE];
    TaskHandle_t  xHandleXbeeRX;

    SemaphoreHandle_t  xHandleSemaphoreRX;
    StaticSemaphore_t  xSemaphoreRx;

    uint8_t  rxBuffer[XBEE_RX_BUFFER_MAX_LENGTH];
    uint8_t  rxPhase;
    uint16_t rxCmdLength;
    uint16_t rxDataToReceive;
    uint16_t rxIndex;

    // -------------------------------------------------------------------------
    // Private methods
    // -------------------------------------------------------------------------

    /**
     * @brief Send data over UART (replaces XBEE_SendData)
     * @param[in] data Pointer to the null-terminated string to send
     * @return XBEE_OK, XBEE_TX_ERROR, or XBEE_TX_TIMEOUT
     */
    int XBEE_SendData(char* data);

    /**
     * @brief Handler task for messages to transmit (replaces XBEE_TxHandlerThread)
     * @return None
     */
    void XBEE_TxHandlerThread(void);

    /**
     * @brief Handler task for message reception (replaces XBEE_RxThread)
     * @return None
     */
    void XBEE_RxThread(void);

    // -------------------------------------------------------------------------
    // FreeRTOS task trampolines
    // -------------------------------------------------------------------------

    /** Trampoline for XBEE_TxHandlerThread */
    static void XBEE_TxHandlerThread_trampoline(void* pvParameters);

    /** Trampoline for XBEE_RxThread */
    static void XBEE_RxThread_trampoline(void* pvParameters);
};

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* XBEE_H */
