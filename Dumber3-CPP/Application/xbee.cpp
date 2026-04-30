/**
 ******************************************************************************
 * @file xbee.cpp
 * @brief xbee driver body (C++ version)
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
#include "main.h"
#include "xbee.h"

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

extern UART_HandleTypeDef    hlpuart1;
extern DMA_HandleTypeDef     hdma_lpuart1_tx;
extern DMA_HandleTypeDef     hdma_lpuart1_rx;

// =========================================================================
// Constructor
// =========================================================================

Xbee::Xbee()
    : xHandleXbeeTXHandler(NULL),
	  xHandleSemaphoreTX(NULL),
	  txIndex(0),
	  txRemainingData(0),
	  txDataToSend(0),
      xHandleXbeeRX(NULL),
      xHandleSemaphoreRX(NULL),
      rxPhase(0),
      rxCmdLength(0),
      rxDataToReceive(0),
      rxIndex(0)
{
    memset(txBuffer, 0, sizeof(txBuffer));
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

// =========================================================================
// FreeRTOS task trampolines
// =========================================================================

void Xbee::XBEE_TxHandlerThread_trampoline(void* pvParameters) {
    static_cast<Xbee*>(pvParameters)->XBEE_TxHandlerThread();
}

void Xbee::XBEE_RxThread_trampoline(void* pvParameters) {
    static_cast<Xbee*>(pvParameters)->XBEE_RxThread();
}

// =========================================================================
// Public API
// =========================================================================

/**
 * @brief  Function for initializing xbee system
 *
 * @param  None
 * @return None
 */
void Xbee::XBEE_Init(void) {
    xHandleSemaphoreTX = xSemaphoreCreateBinaryStatic(&xSemaphoreTX);
    xSemaphoreGive(xHandleSemaphoreTX);

    xHandleSemaphoreRX = xSemaphoreCreateBinaryStatic(&xSemaphoreRx);

    /* Add semaphores to registry in order to view them in STM32CubeIDE */
    vQueueAddToRegistry(xHandleSemaphoreTX, "XBEE TX sem");
    vQueueAddToRegistry(xHandleSemaphoreRX, "XBEE RX sem");

    /* Create the task without using any dynamic memory allocation. */
    xHandleXbeeRX = xTaskCreateStatic(
            XBEE_RxThread_trampoline,   /* Function that implements the task. */
            "XBEE Rx",                  /* Text name for the task. */
            STACK_SIZE,                 /* Number of indexes in the xStack array. */
            this,                       /* Parameter passed into the task (pointer to instance). */
            PriorityXbeeRX,            /* Priority at which the task is created. */
            xStackXbeeRX,              /* Array to use as the task's stack. */
            &xTaskXbeeRX);             /* Variable to hold the task's data structure. */
    vTaskResume(xHandleXbeeRX);

    /* Create the task without using any dynamic memory allocation. */
    xHandleXbeeTXHandler = xTaskCreateStatic(
            XBEE_TxHandlerThread_trampoline, /* Function that implements the task. */
            "XBEE Tx",                       /* Text name for the task. */
            STACK_SIZE,                      /* Number of indexes in the xStack array. */
            this,                            /* Parameter passed into the task (pointer to instance). */
            PriorityXbeeTX,                 /* Priority at which the task is created. */
            xStackXbeeTXHandler,            /* Array to use as the task's stack. */
            &xTaskXbeeTXHandler);           /* Variable to hold the task's data structure. */
    vTaskResume(xHandleXbeeTXHandler);

    /* Enable Xbee */
    HAL_GPIO_WritePin(XBEE_RESET_GPIO_Port, XBEE_RESET_Pin, GPIO_PIN_SET);
}

// =========================================================================
// Interrupt handlers (public, called from global ISR stubs)
// =========================================================================

/**
 * @brief Transmission interrupt handler
 *
 *        This ISR is called when the USART transmit register is empty and ready
 *        for a new char to be sent. A semaphore signals end-of-transmission to
 *        XBEE_SendData.
 *
 * @param None
 * @return None
 */
void Xbee::XBEE_TX_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (txRemainingData == 0) {
        /* No more data — disable TXE interrupt and release semaphore */
        LL_USART_DisableIT_TXE(hlpuart1.Instance);
        xSemaphoreGiveFromISR(xHandleSemaphoreTX, &xHigherPriorityTaskWoken);
    } else {
        LL_USART_TransmitData8(hlpuart1.Instance, txBuffer[txIndex]);
        txIndex++;
        txRemainingData--;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief Reception interrupt handler
 *
 *        This ISR is called when the USART receive register is full with a newly
 *        received char. A semaphore signals end-of-frame reception to XBEE_RxThread.
 *
 * @warning XBEE MODULES FROM S1 GENERATION DON'T WORK (robot uses 2.5V).
 *          Use at least S2 generation modules. Symptom of S1 usage: only two '0'
 *          chars are received, then nothing.
 *
 * @param None
 * @return None
 */
void Xbee::XBEE_RX_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data;

    data = LL_USART_ReceiveData8(hlpuart1.Instance);
    /*
     * If you only receive '0' chars (then nothing), check your XBEE module generation.
     * XBEE MODULES FROM S1 GENERATION DON'T WORK because the robot uses 2.5V.
     * Use at least S2 generation modules.
     */

    if (data != XBEE_ENDING_CHAR) {
        /* End of command not yet received */
        if (data != 0x00) {
            rxBuffer[rxIndex] = data;

            rxIndex++;
            if (rxIndex >= XBEE_RX_BUFFER_MAX_LENGTH)
                rxIndex = 0;

            rxCmdLength++;
            if (rxCmdLength >= XBEE_RX_BUFFER_MAX_LENGTH)
                rxCmdLength = 0;
        }
    } else {
        /* End of command received */
        rxBuffer[rxIndex] = 0; /* null-terminate the C string */
        xSemaphoreGiveFromISR(xHandleSemaphoreRX, &xHigherPriorityTaskWoken);
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief IRQ handler for UART.
 *
 * Dispatches the IRQ event to XBEE_TX_IRQHandler, XBEE_RX_IRQHandler,
 * or clears error flags as appropriate.
 */
void Xbee::LPUART1_IRQHandler(void) {
    if (LL_USART_IsActiveFlag_RXNE(hlpuart1.Instance)) {
        XBEE_RX_IRQHandler();
    } else if ((LL_USART_IsEnabledIT_TXE(hlpuart1.Instance))
            && (LL_USART_IsActiveFlag_TXE(hlpuart1.Instance))) {
        XBEE_TX_IRQHandler();
    } else {
        if ((LL_USART_IsEnabledIT_TC(hlpuart1.Instance))
                && (LL_USART_IsActiveFlag_TC(hlpuart1.Instance))) {
            LL_USART_DisableIT_TC(hlpuart1.Instance);
        } else if ((LL_USART_IsEnabledIT_IDLE(hlpuart1.Instance))
                && (LL_USART_IsActiveFlag_IDLE(hlpuart1.Instance))) {
            LL_USART_ClearFlag_IDLE(hlpuart1.Instance);
        } else {
            LL_USART_ClearFlag_ORE(hlpuart1.Instance);
            LL_USART_ClearFlag_FE(hlpuart1.Instance);
            LL_USART_ClearFlag_PE(hlpuart1.Instance);
            LL_USART_ClearFlag_NE(hlpuart1.Instance);
            LL_USART_DisableIT_ERROR(hlpuart1.Instance);
            LL_USART_DisableIT_PE(hlpuart1.Instance);
        }
    }
}

// =========================================================================
// Private methods
// =========================================================================

/**
 * @brief Handler task for messages to transmit (send to supervisor)
 *        Manages XBEE mailbox and sends messages.
 *
 * @return None
 */
void Xbee::XBEE_TxHandlerThread(void) {
    MESSAGE_Typedef msg;

    while (1) {
        msg = MESSAGE_ReadMailbox(XBEE_Mailbox);

        if (msg.id == MSG_ID_XBEE_ANS) {
            XBEE_SendData((char*) msg.data); /* blocking during send */
            free(msg.data);
        }
    }
}

/**
 * @brief Send data.
 *
 * Creates a transmission frame, adds escape chars to data, and sends it over UART.
 *
 * @remark Non-blocking unless another transmission is still in progress.
 *
 * @param[in] data Pointer to the raw null-terminated string to send
 * @return Status:
 * - XBEE_OK           if sending started successfully
 * - XBEE_TX_ERROR     in case of a sending error
 * - XBEE_TX_TIMEOUT   if the semaphore timed out
 */
int Xbee::XBEE_SendData(char* data) {
    BaseType_t state;
    int status = XBEE_OK;

    /* Prevents successive calls from overlapping */
    state = xSemaphoreTake(xHandleSemaphoreTX,
            pdMS_TO_TICKS(XBEE_TX_SEMAPHORE_WAIT)); /* wait max 500 ms */

    if (state != pdFALSE) {
        strncpy((char*) txBuffer, data, XBEE_TX_BUFFER_MAX_LENGTH - 1);
        txBuffer[XBEE_TX_BUFFER_MAX_LENGTH - 1] = 0;
        txRemainingData = strlen((char*) txBuffer);

        if (txRemainingData != 0) {
            txIndex         = 1;
            txRemainingData = txRemainingData - 1;

            LL_USART_TransmitData8(hlpuart1.Instance, txBuffer[0]);
            LL_USART_EnableIT_TXE(hlpuart1.Instance); /* enable TX interrupt */
        }
    } else {
        status = XBEE_TX_TIMEOUT;
    }

    return status;
}

/**
 * @brief Handler task for message reception (received from supervisor)
 *
 * Waits for incoming messages and forwards them to the application mailbox.
 *
 * @warning XBEE MODULES FROM S1 GENERATION DON'T WORK (robot uses 2.5V).
 *          Use at least S2 generation modules. If you receive only two '0' chars
 *          then nothing, check the module generation.
 *
 * @return None
 */
void Xbee::XBEE_RxThread(void) {
    char* incomingData;
    rxCmdLength = 0;
    rxIndex     = 0;

    while (HAL_UART_Receive_IT(&hlpuart1, rxBuffer, 1) != HAL_OK);
    LL_USART_Disable(hlpuart1.Instance);
    LL_USART_DisableOverrunDetect(hlpuart1.Instance);
    LL_USART_Enable(hlpuart1.Instance);

    /* Endless reception loop */
    while (1) {
        if (xSemaphoreTake(xHandleSemaphoreRX, portMAX_DELAY) == pdTRUE) {

            if (rxCmdLength > XBEE_RX_BUFFER_MAX_LENGTH)
                rxCmdLength = XBEE_RX_BUFFER_MAX_LENGTH;

            incomingData = (char*) malloc(rxCmdLength + 1); /* +1 for null terminator */
            strncpy(incomingData, (char*) rxBuffer, rxCmdLength + 1);

            rxCmdLength = 0; /* reset counters for next command */
            rxIndex     = 0;

            MESSAGE_SendMailbox(APPLICATION_Mailbox, MSG_ID_XBEE_CMD,
                    (QueueHandle_t) 0x0, (void*) incomingData);
        }
    }
}

/**
 * @}
 */

/**
 * @}
 */
