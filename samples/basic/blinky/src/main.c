/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

// Add a simple shell command that does nothing

#include <zephyr/shell/shell.h>

#include "fsl_device_registers.h"

static int cmd_rebootinfo(const struct shell *shell, size_t argc, char **argv)
{
	const uint32_t reset_stat_reg = RSTCTL0->SYSRSTSTAT;
	shell_print(shell, "Reset status register: 0x%08x", reset_stat_reg);
	if (reset_stat_reg & RSTCTL0_SYSRSTSTAT_VDD_POR_MASK) {
		shell_print(shell, "VDD_POR");
	}
	if (reset_stat_reg & RSTCTL0_SYSRSTSTAT_PAD_RESET_MASK) {
		shell_print(shell, "PAD_RESET");
	}
	if (reset_stat_reg & RSTCTL0_SYSRSTSTAT_ARM_APD_RESET_MASK) {
		shell_print(shell, "ARM_APD_RESET");
	}
	if (reset_stat_reg & RSTCTL0_SYSRSTSTAT_WDT0_RESET_MASK) {
		shell_print(shell, "WDT0_RESET");
	}
	if (reset_stat_reg & RSTCTL0_SYSRSTSTAT_WDT1_RESET_MASK) {
		shell_print(shell, "WDT1_RESET");
	}
	return 0;
}
SHELL_CMD_REGISTER(rebootinfo, NULL, "Print reboot info", cmd_rebootinfo);

// simple command to clear reboot info
static int cmd_clearrebootinfo(const struct shell *shell, size_t argc, char **argv)
{
	RSTCTL0->SYSRSTSTAT = 0xFFFFFFFF;
	shell_print(shell, "Reset status register cleared");
	return 0;
}
SHELL_CMD_REGISTER(clearrebootinfo, NULL, "Clear reboot info", cmd_clearrebootinfo);

int main(void)
{
	int ret;
	bool led_state = true;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		// printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
