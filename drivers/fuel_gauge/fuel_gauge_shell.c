/*
 * Copyright 2025 Noah Pendleton
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/fuel_gauge.h>

#include <zephyr/shell/shell.h>

// BUILD_ASSERT(DEVICE_DT_GET_ANY(silergy_sy24561) != NULL, "Error: be sure to enable a fuel gauge device");

const struct device *fuel_gauge_dev = DEVICE_DT_GET_ANY(silergy_sy24561);

static const char* prv_fuel_gauge_prop_to_string(fuel_gauge_prop_t prop)
{
#define FUEL_GAUGE_PROP_CASE(x) case x: return #x;
	switch (prop) {
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_AVG_CURRENT)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CURRENT)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CHARGE_CUTOFF)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CYCLE_COUNT)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CONNECT_STATE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_FLAGS)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_FULL_CHARGE_CAPACITY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_PRESENT_STATE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_REMAINING_CAPACITY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_RUNTIME_TO_EMPTY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_RUNTIME_TO_FULL)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_MFR_ACCESS)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_ABSOLUTE_STATE_OF_CHARGE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_RELATIVE_STATE_OF_CHARGE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_TEMPERATURE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_VOLTAGE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_MODE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CHARGE_CURRENT)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CHARGE_VOLTAGE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_STATUS)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_DESIGN_CAPACITY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_DESIGN_VOLTAGE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_ATRATE)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_ATRATE_TIME_TO_FULL)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_ATRATE_TIME_TO_EMPTY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_ATRATE_OK)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_REMAINING_CAPACITY_ALARM)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_SBS_REMAINING_TIME_ALARM)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_MANUFACTURER_NAME)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_DEVICE_NAME)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_DEVICE_CHEMISTRY)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_CURRENT_DIRECTION)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_STATE_OF_CHARGE_ALARM)
		FUEL_GAUGE_PROP_CASE(FUEL_GAUGE_LOW_VOLTAGE_ALARM)
		default:
			return "Unknown";
	}
#undef FUEL_GAUGE_PROP_CASE
}

static int cmd_get_fuel_gauge_props(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* For each possible prop, attempt to read and print, or n/a if error */
	for (int i = 0; i < FUEL_GAUGE_COMMON_COUNT; i++) {
		union fuel_gauge_prop_val val;
		if (fuel_gauge_get_prop(fuel_gauge_dev, i, &val) == 0) {
			/* the individual properties have various types. for simplicity
			we'll render them all as uint32_t. */
			shell_print(shell, "  %s: %u", prv_fuel_gauge_prop_to_string(i), (uint32_t)val.cycle_count);
		} else {
			shell_print(shell, "  %s: n/a", prv_fuel_gauge_prop_to_string(i));
		}
	}

	return 0;
}

/* clang-format off */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_fuel_gauge,
	SHELL_CMD(get_props, NULL, "Read all fuel gauge props", cmd_get_fuel_gauge_props),
	SHELL_SUBCMD_SET_END
	);
/* clang-format on */

SHELL_CMD_REGISTER(fuel_gauge, &sub_fuel_gauge, "Fuel gauge commands", NULL);
