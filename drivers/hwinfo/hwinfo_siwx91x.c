/*
 * Copyright (c) 2025 Noah Pendleton
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/sys/util.h>

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	/*
	The SiWX91x chips include an undocumented memory-mapped "efusecopy"
	region that contains the factory programmed BLE and WiFi MAC values. The
	offsets for these values were experimentally derived by using the SiLabs
	"Simplicity Commander" utility on an siwx917_rb4338a board, specifically
	these commands:

	Read device info:
	$ commander device info
		Part Number    : SiWG917M111MGTBA
		Product Rev    : B0
		Flash Size     : 8192 kB
		SRAM Size      : 672 kB
		Unique ID      : 0000d448671c1504
		DONE

	Dump the manufacturing data, which includes the "efusecopy" region:
	$ commander mfg917 dump data.zip
	*/
	uint8_t *wifi_mac = (uint8_t *)(0x040003E0 + 0x22);
	const ssize_t id_length = MIN(6, length);
	memcpy(buffer, wifi_mac, id_length);

	return id_length;
}

int z_impl_hwinfo_get_supported_reset_cause(uint32_t *supported)
{
	*supported = (RESET_SOFTWARE | RESET_POR);

	return 0;
}

int z_impl_hwinfo_get_reset_cause(uint32_t *cause)
{
	/*
	The SiWx91x chips have 2 bits for tracking reset cause:

	- MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS_b.MCU_FIRST_POWERUP_POR
	    Should be set by software on system startup. Cleared in hardware after
	    Vbatt power is removed.

	- MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS_b.MCU_FIRST_POWERUP_RESET_N
	    Should be set by software on system startup. Cleared in hardware when
	    reset pin is pulled low

	The values of these bits on reset are stored in the complementary
	MCU_FSM_WAKEUP_STATUS_REG bits
	*/

	printk("MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS: 0x%08x\n",
	       MCU_AON->MCUAON_KHZ_CLK_SEL_POR_RESET_STATUS);
	extern uint32_t siwx917_reset_status;
	printk("siwx917_reset_status: 0x%08x\n",
		siwx917_reset_status);

	/* If the POR bit was cleared, this is a POR */
	if (MCU_FSM->MCU_FSM_WAKEUP_STATUS_REG_b.MCU_FIRST_POWERUP_POR == 0) {
		*cause = RESET_POR;
		/* If the First Powerup Reset bit is cleared, this was a reset pin reset */
	} else if (MCU_FSM->MCU_FSM_WAKEUP_STATUS_REG_b.MCU_FIRST_POWERUP_RESET_N == 0) {
		*cause = RESET_PIN;
		/* Otherwise, if both bits were set, this is a soft reset */
	} else {
		*cause = RESET_SOFTWARE;
	}

	return 0;
}

int z_impl_hwinfo_clear_reset_cause(void)
{
	/* No-op. */
	return 0;
}
