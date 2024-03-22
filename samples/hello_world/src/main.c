/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/toolchain.h>
#include <zephyr/kernel.h>
#include <app_version.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	printk("Kconfig available APP_VERSION identifiers:\n");
	printk("%s\n", CONFIG_APP_VERSION);

	return 0;
}
