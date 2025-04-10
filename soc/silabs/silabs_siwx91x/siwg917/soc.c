/*
 * Copyright (c) 2023 Antmicro
 * Copyright (c) 2024 Silicon Laboratories Inc.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sw_isr_table.h>

#include <soc.h>

#include "em_device.h"

uint32_t siwx917_reset_status;

void soc_early_init_hook(void)
{
	printk("MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS: 0x%08x\n",
		MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS);
	siwx917_reset_status = MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS;

	SystemInit();
}

/* SiWx917's bootloader requires IRQn 32 to hold payload's entry point address. */
extern void z_arm_reset(void);
Z_ISR_DECLARE_DIRECT(32, 0, z_arm_reset);
