/***************************************************************************//**
* \file main_cm0p.c
* \version 1.30
*
* This file provides App0 Core0 example source.
* App0 Core0 firmware does the following:
* - Starts App0 Core1 firmware.
* - If required switches to App1.
*
********************************************************************************
* \copyright
* Copyright 2017-2019, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions,
* disclaimers, and limitations in the end user license agreement accompanying
* the software package with which this file was provided.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_dfu.h"

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
    cy_stc_crypto_server_context_t  myCryptoServerContext;
#endif

/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  Main function of App#0 core0. Initializes core1 (CM4) and waits forever.
*
* Parameters:
*  None
* 
* Return:
*  None
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* enable global interrupts */
    __enable_irq();
    
#if CY_DFU_OPT_CRYPTO_HW != 0
    cy_en_crypto_status_t           cryptoStatus;
    /* Start the Crypto Server */
    cryptoStatus = Cy_Crypto_Server_Start_Base(&myCryptoConfig, &myCryptoServerContext);
    if(CY_CRYPTO_SUCCESS != cryptoStatus)
    {
        CY_ASSERT(0);
    }
#endif

    /* start up M4 core, with the CM4 core start address defined in the
       DFU SDK linker script */
    Cy_SysEnableCM4( (uint32_t)(&__cy_app_core1_start_addr) );

    for (;;)
    {
        /* empty */
    }
}

/*******************************************************************************
* Function Name: Cy_OnResetUser
********************************************************************************
*
* Summary:
*  This function is called at the start of Reset_Handler(). It is a weak
*  function that may be redefined by user code.
*  The DFU SDK requires this function to call Cy_DFU_OnResetApp0() in app#0
*  core0.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void Cy_OnResetUser(void)
{
    Cy_DFU_OnResetApp0();
}

/* [] END OF FILE */
