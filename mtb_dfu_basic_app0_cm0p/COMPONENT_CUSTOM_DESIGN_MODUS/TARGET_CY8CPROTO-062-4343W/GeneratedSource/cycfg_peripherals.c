/*******************************************************************************
* File Name: cycfg_peripherals.c
*
* Description:
* Peripheral Hardware Block configuration
* This file was automatically generated and should not be modified.
* Device Configurator: 2.0.0.1483
* Device Support Library (../../../libs/psoc6pdl): 1.4.0.1889
*
********************************************************************************
* Copyright 2017-2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/

#include "cycfg_peripherals.h"

cy_stc_csd_context_t cy_csd_0_context = 
{
	.lockKey = CY_CSD_NONE_KEY,
};
const cy_stc_scb_i2c_config_t DFU_I2C_config = 
{
	.i2cMode = CY_SCB_I2C_SLAVE,
	.useRxFifo = true,
	.useTxFifo = true,
	.slaveAddress = 8,
	.slaveAddressMask = 254,
	.acceptAddrInFifo = false,
	.ackGeneralAddr = false,
	.enableWakeFromSleep = false,
	.enableDigitalFilter = false,
	.lowPhaseDutyCycle = 0,
	.highPhaseDutyCycle = 0,
};
#if defined (CY_USING_HAL)
	const cyhal_resource_inst_t DFU_I2C_obj = 
	{
		.type = CYHAL_RSC_SCB,
		.block_num = 3U,
		.channel_num = 0U,
	};
#endif //defined (CY_USING_HAL)


void init_cycfg_peripherals(void)
{
	Cy_SysClk_PeriphAssignDivider(PCLK_CSD_CLOCK, CY_SYSCLK_DIV_8_BIT, 0U);

	Cy_SysClk_PeriphAssignDivider(PCLK_SCB3_CLOCK, CY_SYSCLK_DIV_8_BIT, 1U);
#if defined (CY_USING_HAL)
	cyhal_hwmgr_reserve(&DFU_I2C_obj);
#endif //defined (CY_USING_HAL)
}
