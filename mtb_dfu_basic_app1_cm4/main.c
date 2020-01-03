/***************************************************************************//**
* \file main_cm4.c
* \version 1.30
*
* This file provides App1 Core1 example source.
* App1 Core1 firmware does the following:
* - Blinks a LED
* - Swithes to App0 if button is pressed
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


/* This section is used to verify an application signature.
   For checksum verification, set the number of elements in the array to 1, and
   in the linker script files for both cores, common section, set
   __cy_boot_signature_size = 4.
*/
CY_SECTION(".cy_app_signature") __USED static const uint32_t cy_appSignature[1];


/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
*  Main function of Application#1 core1 (CM4).
*  Blinks a LED, when button is pressed and released it switches to app#0 for
*  new app#1 to be downloaded.  
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
    /* Enable global interrupts */
    __enable_irq();

    for(;;)
    {
        /* Blink twice per second */
        Cy_GPIO_Inv(PIN_LED);
        Cy_SysLib_Delay(250u);
        
        /* If Button clicked and switch to App0 */
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
                
                Cy_DFU_ExecuteApp(0u);
            }
        }
    }
}

/* [] END OF FILE */