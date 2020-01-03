/***************************************************************************//**
* \file main_cm4.c
* \version 1.30
*
* This file provides App0 Core1 example source.
* App0 Core1 firmware does the following:
* - Downloads App1 firmware image if Host sends it
* - Switches to App1 if App1 image has successfully downloaded and is valid
* - Switches to existing App1 if button is pressed
* - Blinks a LED
* - Halts on timeout
*
********************************************************************************
* \copyright
* Copyright 2016-2017, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_dfu.h"
#include <string.h>

/*
* For usage with Cy_GPIO_Read(PIN_SW2) and Cy_GPIO_Write(PIN_SW2, value)
* instead of Cy_GPII_Read(GPIO_PTR0, 4u) and Cy_GPIO_Write(GPIO_PTR0, 4u, value)
*
* Pin for user button SW2.
*/
#define PIN_SW2     GPIO_PRT0, 4u

/*
* For usage with Cy_GPIO_Inv(PIN_LED) instead of Cy_GPIO_Inv(GPIO_PRT0, 3u).
* Defines a red LED pin's "port" and "pin number".
*/
#define PIN_LED     GPIO_PRT13, 7u

#if CY_DFU_OPT_CRYPTO_HW != 0
    /* Scenario: Configure Server and Client as follows:
     * Server:
     *  - IPC channel for communication: 9
     *  - IPC interrupt structure used for new request notifications: 1
     *  - Data request handler: default
     *  - Hardware errors handler: default
     * Client:
     *  - IPC channel for communication: 9
     *  - IPC interrupt structure used for data ready notifications: 2
     *  - Data Complete callback: not used
     */
    #define MY_CHAN_CRYPTO         (uint32_t)(9u)    /* IPC data channel for the Crypto */
    #define MY_INTR_CRYPTO_SRV     (uint32_t)(1u)    /* IPC interrupt structure for the Crypto server */
    #define MY_INTR_CRYPTO_CLI     (uint32_t)(2u)    /* IPC interrupt structure for the Crypto client */
    #define MY_INTR_CRYPTO_SRV_MUX (IRQn_Type)(2u)   /* CM0+ IPC interrupt mux number the Crypto server */
    #define MY_INTR_CRYPTO_CLI_MUX (IRQn_Type)(3u)   /* CM0+ IPC interrupt mux number the Crypto client */
    #define MY_INTR_CRYPTO_ERR_MUX (IRQn_Type)(4u)   /* CM0+ ERROR interrupt mux number the Crypto server */
    const cy_stc_crypto_config_t myCryptoConfig =
    {
        /* .ipcChannel             */ MY_CHAN_CRYPTO,
        /* .acquireNotifierChannel */ MY_INTR_CRYPTO_SRV,
        /* .releaseNotifierChannel */ MY_INTR_CRYPTO_CLI,
        /* .releaseNotifierConfig */ {
        #if (CY_CPU_CORTEX_M0P)
            /* .intrSrc            */ MY_INTR_CRYPTO_CLI_MUX,
            /* .cm0pSrc            */ cpuss_interrupts_ipc_2_IRQn, /* depends on selected releaseNotifierChannel value */
        #else
            /* .intrSrc            */ cpuss_interrupts_ipc_2_IRQn, /* depends on selected releaseNotifierChannel value */
        #endif
            /* .intrPriority       */ 2u,
        },
        /* .userCompleteCallback   */ NULL,
        /* .userGetDataHandler     */ NULL,
        /* .userErrorHandler       */ NULL,
        /* .acquireNotifierConfig */ {
        #if (CY_CPU_CORTEX_M0P)
            /* .intrSrc            */ MY_INTR_CRYPTO_SRV_MUX,      /* to use with DeepSleep mode should be in DeepSleep capable muxer's range */
            /* .cm0pSrc            */ cpuss_interrupts_ipc_1_IRQn, /* depends on selected acquireNotifierChannel value */
        #else
            /* .intrSrc            */ cpuss_interrupts_ipc_1_IRQn, /* depends on selected acquireNotifierChannel value */
        #endif
            /* .intrPriority       */ 2u,
        },
        /* .cryptoErrorIntrConfig */ {
        #if (CY_CPU_CORTEX_M0P)
            /* .intrSrc            */ MY_INTR_CRYPTO_ERR_MUX,
            /* .cm0pSrc            */ cpuss_interrupt_crypto_IRQn,
        #else
            /* .intrSrc            */ cpuss_interrupt_crypto_IRQn,
        #endif
            /* .intrPriority       */ 2u,
        }
    };
    cy_stc_crypto_context_t     myCryptoContext;
#endif /* CY_DFU_OPT_CRYPTO_HW != 0 */


/*******************************************************************************
* Function Name: CopyRow
********************************************************************************
* Copies data from a "src" address to a flash row with the address "dest".
* If "src" data is the same as "dest" data then no copy is needed.
*
* Parameters:
*  dest     Destination address. Has to be an address of the start of flash row.
*  src      Source address. Has to be properly aligned.
*  rowSize  Size of flash row.
*
* Returns:
*  CY_DFU_SUCCESS if operation is successful.
*  Error code in a case of failure.
*******************************************************************************/
cy_en_dfu_status_t CopyRow(uint32_t dest, uint32_t src, uint32_t rowSize, cy_stc_dfu_params_t * params)
{
    cy_en_dfu_status_t status;
    
    /* Save params->dataBuffer value */
    uint8_t *buffer = params->dataBuffer;

    /* Compare "dest" and "src" content */
    params->dataBuffer = (uint8_t *)src;
    status = Cy_DFU_ReadData(dest, rowSize, CY_DFU_IOCTL_COMPARE, params);
    
    /* Restore params->dataBuffer */
    params->dataBuffer = buffer;

    /* If "dest" differs from "src" then copy "src" to "dest" */
    if (status != CY_DFU_SUCCESS)
    {
        (void) memcpy((void *) params->dataBuffer, (const void*)src, rowSize);
        status = Cy_DFU_WriteData(dest, rowSize, CY_DFU_IOCTL_WRITE, params);
    }
    /* Restore params->dataBuffer */
    params->dataBuffer = buffer;
    
    return (status);
}


/*******************************************************************************
* Function Name: HandleMetadata
********************************************************************************
* The goal of this function is to make DFU SDK metadata (MD) valid.
* The following algorithm is used (in C-like pseudocode):
* ---
* if (isValid(MD) == true)
* {   if (MDC != MD)
*         MDC = MD;
* } else
* {   if(isValid(MDC) )
*         MD = MDC;
*     else
*         MD = INITIAL_VALUE;
* }
* ---
* Here MD is metadata flash row, MDC is flash row with metadata copy,
* INITIAL_VALUE is known initial value.
*
* In this code example MDC is placed in the next flash row after the MD, and
* INITIAL_VALUE is MD with only CRC, App0 start and size initialized,
* all the other fields are not touched.
*
* Parameters:
*  params   A pointer to a DFU SDK parameters structure.
*
* Returns:
* - CY_DFU_SUCCESS when finished normally.
* - Any other status code on error.
*******************************************************************************/
cy_en_dfu_status_t HandleMetadata(cy_stc_dfu_params_t *params)
{
    const uint32_t MD     = (uint32_t)(&__cy_boot_metadata_addr   ); /* MD address  */
    const uint32_t mdSize = (uint32_t)(&__cy_boot_metadata_length ); /* MD size, assumed to be one flash row */
    const uint32_t MDC    = MD + mdSize;                             /* MDC address */

    cy_en_dfu_status_t status = CY_DFU_SUCCESS;
    
    status = Cy_DFU_ValidateMetadata(MD, params);
    if (status == CY_DFU_SUCCESS)
    {
        /* Checks if MDC equals to DC, if no then copies MD to MDC */
        status = CopyRow(MDC, MD, mdSize, params);
    }
    else
    {
        status = Cy_DFU_ValidateMetadata(MDC, params);
        if (status == CY_DFU_SUCCESS)
        {
            /* Copy MDC to MD */
            status = CopyRow(MD, MDC, mdSize, params);
        }
        if (status != CY_DFU_SUCCESS)
        {
            const uint32_t elfStartAddress = 0x10000000;
            const uint32_t elfAppSize      = 0x8000;
            /* Set MD to INITIAL_VALUE */
            status = Cy_DFU_SetAppMetadata(0u, elfStartAddress, elfAppSize, params);
        }
    }
    return (status);
}


/*******************************************************************************
* Function Name: counterTimeoutSeconds
********************************************************************************
* Returns number of counts that correspond to number of seconds passed as
* a parameter.
* E.g. comparing counter with 300 seconds is like this.
* ---
* uint32_t counter = 0u;
* for (;;)
* {
*     Cy_SysLib_Delay(UART_TIMEOUT);
*     ++count;
*     if (count >= counterTimeoutSeconds(seconds: 300u, timeout: UART_TIMEOUT))
*     {
*         count = 0u;
*         DoSomething();
*     }
* }
* ---
*
* Both parameters are required to be compile time constants,
* so this function gets optimized out to single constant value.
*
* Parameters:
*  seconds    Number of seconds to pass. Must be less that 4_294_967 seconds.
*  timeout    Timeout for Cy_DFU_Continue() function, in milliseconds.
*             Must be greater than zero.
*             It is recommended to be a value that produces no reminder
*             for this function to be precise.
* Return:
*  See description.
*******************************************************************************/
static uint32_t counterTimeoutSeconds(uint32_t seconds, uint32_t timeout);
static uint32_t counterTimeoutSeconds(uint32_t seconds, uint32_t timeout)
{
    return (seconds * 1000ul) / timeout;
}


/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  Main function of the firmware application.
*  1. If application started from Non-Software reset it validates app #1
*  1.1. If app#1 is valid it switches to app#1, else goto #2.
*  2. Start DFU communication.
*  3. If updated application has been received it validates this app.
*  4. If app#1 is valid it switches to it, else wait for new application.
*  5. If 300 seconds has passed and no new application has been received
*     then validate app#1, if it is valid then switch to it, else freeze.
*
* Parameters:
*  seconds    Number of seconds to pass
*  timeout    Timeout for Cy_DFU_Continue() function, in milliseconds
* 
* Return:
*  Counter value at which specified number of seconds has passed.
*
*******************************************************************************/
int main(void)
{
    /* timeout for Cy_DFU_Continue(), in milliseconds */
    const uint32_t paramsTimeout = 20u;
    
    /* DFU params, used to configure DFU */
    cy_stc_dfu_params_t dfuParams;
    
    /* Status codes for DFU SDK API */
    cy_en_dfu_status_t status;
    
    /* 
    * DFU state, one of
    * - CY_DFU_STATE_NONE
    * - CY_DFU_STATE_UPDATING
    * - CY_DFU_STATE_FINISHED
    * - CY_DFU_STATE_FAILED
    */
    uint32_t state;

    /*
    * Used to count seconds, to convert counts to seconds use
    * counterTimeoutSeconds(SECONDS, paramsTimeout)
    */
    uint32_t count = 0;
    
#if CY_DFU_OPT_CRYPTO_HW != 0
    cy_en_crypto_status_t cryptoStatus;
#endif

    /* Buffer to store DFU commands */
    CY_ALIGN(4) static uint8_t buffer[CY_DFU_SIZEOF_DATA_BUFFER];

    /* Buffer for DFU packets for Transport API */
    CY_ALIGN(4) static uint8_t packet[CY_DFU_SIZEOF_CMD_BUFFER ];    
    
    /* Enable global interrupts */
    __enable_irq();
    
#if CY_DFU_OPT_CRYPTO_HW != 0
    /* Initialize the Crypto Client code */
    cryptoStatus = Cy_Crypto_Init(&myCryptoConfig, &myCryptoContext);
    if (cryptoStatus != CY_CRYPTO_SUCCESS)
    {
        /* Crypto not initialized, debug what is the problem */
        Cy_SysLib_Halt(0x00u);
    }
    cryptoStatus = Cy_Crypto_Enable();
    if (cryptoStatus != CY_CRYPTO_SUCCESS)
    {
        /* Crypto not initialized, debug what is the problem */
        Cy_SysLib_Halt(0x00u);
    }
#endif /* CY_DFU_OPT_CRYPTO_HW != 0 */

    /* Initialize dfuParams structure and DFU SDK state */
    dfuParams.timeout          = paramsTimeout;
    dfuParams.dataBuffer       = &buffer[0];
    dfuParams.packetBuffer     = &packet[0];

    status = Cy_DFU_Init(&state, &dfuParams);

    /* Ensure DFU Metadata is valid */
    status = HandleMetadata(&dfuParams);
    if (status != CY_DFU_SUCCESS)
    {
        Cy_SysLib_Halt(0x00u);
    }
    
    /*
    * In the case of non-software reset check if there is a valid app image.
    * If these is - switch to it.
    */
    if (Cy_SysLib_GetResetReason() != CY_SYSLIB_RESET_SOFT)
    {
        status = Cy_DFU_ValidateApp(1u, &dfuParams);
        if (status == CY_DFU_SUCCESS)
        {
            /*
            * Clear the reset reason because Cy_DFU_ExecuteApp() performs a 
            * software reset. Without clearing it, two reset reasons would be 
            * present.
            */
            do
            {
                Cy_SysLib_ClearResetReason();
            }while(Cy_SysLib_GetResetReason() != 0);

            /* Never returns */
            Cy_DFU_ExecuteApp(1u);
        }
    }
    
    /* Initialize DFU communication */
    Cy_DFU_TransportStart();
    
    for(;;)
    {
        status = Cy_DFU_Continue(&state, &dfuParams);
        ++count;

        if (state == CY_DFU_STATE_FINISHED)
        {
            /* Finished downloading the application image */
            
            /* Validate downloaded application, if it is valid then switch to it */
            status = Cy_DFU_ValidateApp(1u, &dfuParams);
            if (status == CY_DFU_SUCCESS)
            {
                Cy_DFU_TransportStop();
                Cy_DFU_ExecuteApp(1u);
            }
            else if (status == CY_DFU_ERROR_VERIFY)
            {
                /*
                * Restarts DFU, an alternatives are to Halt MCU here
                * or switch to the other app if it is valid.
                * Error code may be handled here, i.e. print to debug UART.
                */
                status = Cy_DFU_Init(&state, &dfuParams);
                Cy_DFU_TransportReset();
            }
        }
        else if (state == CY_DFU_STATE_FAILED)
        {
            /* An error has happened during the DFU process */
            /* Handle it here */
            
            /* In this Code Example just restart DFU process */
            status = Cy_DFU_Init(&state, &dfuParams);
            Cy_DFU_TransportReset();
        }
        else if (state == CY_DFU_STATE_UPDATING)
        {
            uint32_t passed5seconds = (count >= counterTimeoutSeconds(5u, paramsTimeout) ) ? 1u : 0u;
            /*
            * if no command has been received during 5 seconds when DFU
            * has started then restart DFU.
            */
            if (status == CY_DFU_SUCCESS)
            {
                count = 0u;
            }
            else if (status == CY_DFU_ERROR_TIMEOUT)
            {
                if (passed5seconds != 0u)
                {
                    count = 0u;
                    Cy_DFU_Init(&state, &dfuParams);
                    Cy_DFU_TransportReset();
                }
            }
            else
            {
                count = 0u;
                /* Delay because Transport still may be sending error response to a host */
                Cy_SysLib_Delay(paramsTimeout);
                Cy_DFU_Init(&state, &dfuParams);
                Cy_DFU_TransportReset();
            }
        }

        /* No image has been received in 300 seconds, try to load existing image, or sleep */
        if( (count >= counterTimeoutSeconds(300u, paramsTimeout) ) && (state == CY_DFU_STATE_NONE) )
        {
            /* Stop DFU communication */
            Cy_DFU_TransportStop();
            /* Check if app is valid, if it is then switch to it */
            status = Cy_DFU_ValidateApp(1u, &dfuParams);
            if (status == CY_DFU_SUCCESS)
            {
                Cy_DFU_ExecuteApp(1u);
            }
            /* 300 seconds has passed and App is invalid. Handle that */
            Cy_SysLib_Halt(0x00u);
        }
        
        /* Blink once per two seconds */
        if ( ( count % counterTimeoutSeconds(1u, paramsTimeout) ) == 0u) 
        {
            Cy_GPIO_Inv(PIN_LED);
        }

        /* If Button clicked - Switch to App1 if it is valid */
        if (Cy_GPIO_Read(PIN_SW2) == 0u)
        {
            /* 50 ms delay for button debounce on button press */
            Cy_SysLib_Delay(50u);
            
            if (Cy_GPIO_Read(PIN_SW2) == 0u)
            {
                while (Cy_GPIO_Read(PIN_SW2) == 0u)
                {   /* 50 ms delay for button debounce on button release */
                    Cy_SysLib_Delay(50u);
                }
                
                /* Validate and switch to App1 */
                status = Cy_DFU_ValidateApp(1u, &dfuParams);
                
                if (status == CY_DFU_SUCCESS)
                {
                    Cy_DFU_TransportStop();
                    Cy_DFU_ExecuteApp(1u);
                }
            }
        }
    }
}

/* [] END OF FILE */
