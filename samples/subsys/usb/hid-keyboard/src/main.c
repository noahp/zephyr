/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sample_usbd.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

#include <zephyr/logging/log.h>
#include <string.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* MDS Protocol Report IDs */
#define MDS_REPORT_ID_SUPPORTED_FEATURES  1
#define MDS_REPORT_ID_DEVICE_IDENTIFIER   2
#define MDS_REPORT_ID_DATA_URI           3
#define MDS_REPORT_ID_AUTHORIZATION      4
#define MDS_REPORT_ID_STREAM_CONTROL     5
#define MDS_REPORT_ID_STREAM_DATA        96

/* MDS Protocol Constants */
#define MDS_STREAM_MODE_DISABLED         0
#define MDS_STREAM_MODE_ENABLED          1

/* Mock MDS data for testing */
static const uint32_t mds_supported_features = 0x0000001F; /* All features supported */
static const char mds_device_id[] = "nrf52840dk-test-device-001";
static const char mds_data_uri[] = "https://chunks.memfault.com/api/v0/chunks/device-id";
static const char mds_auth_token[] = "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.test.token";
static bool mds_streaming_enabled = false;

static const uint8_t hid_report_desc[] = {
	// Standard keyboard report (no report ID)
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
		HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),
		/* HID_USAGE_MINIMUM(Keyboard LeftControl) */
		HID_USAGE_MIN8(0xE0),
		/* HID_USAGE_MAXIMUM(Keyboard Right GUI) */
		HID_USAGE_MAX8(0xE7),
		HID_LOGICAL_MIN8(0),
		HID_LOGICAL_MAX8(1),
		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(8),
		/* HID_INPUT(Data,Var,Abs) */
		HID_INPUT(0x02),
		HID_REPORT_SIZE(8),
		HID_REPORT_COUNT(1),
		/* HID_INPUT(Cnst,Var,Abs) */
		HID_INPUT(0x03),
		HID_REPORT_SIZE(1),
		HID_REPORT_COUNT(5),
		HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),
		/* HID_USAGE_MINIMUM(Num Lock) */
		HID_USAGE_MIN8(1),
		/* HID_USAGE_MAXIMUM(Kana) */
		HID_USAGE_MAX8(5),
		/* HID_OUTPUT(Data,Var,Abs) */
		HID_OUTPUT(0x02),
		HID_REPORT_SIZE(3),
		HID_REPORT_COUNT(1),
		/* HID_OUTPUT(Cnst,Var,Abs) */
		HID_OUTPUT(0x03),
		HID_REPORT_SIZE(8),
		HID_REPORT_COUNT(6),
		HID_LOGICAL_MIN8(0),
		HID_LOGICAL_MAX8(101),
		HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD),
		/* HID_USAGE_MIN8(Reserved) */
		HID_USAGE_MIN8(0),
		/* HID_USAGE_MAX8(Keyboard Application) */
		HID_USAGE_MAX8(101),
		/* HID_INPUT (Data,Ary,Abs) */
		HID_INPUT(0x00),
	HID_END_COLLECTION,

	// Custom vendor-defined report with Report ID 96 (Stream Data)
	// Usage Page: Vendor-defined (0xFF00)
	0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x01,        // Usage (0x01)
	0xA1, 0x01,        // Collection (Application)
	0x85, 96,          //   Report ID (96) - Stream Data
	0x09, 0x02,        //   Usage (0x02)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x3F,        //   Report Count (63) - 63 bytes for OUT report
	0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0x09, 0x03,        //   Usage (0x03)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x3F,        //   Report Count (63) - 63 bytes for IN report
	0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              // End Collection

	// MDS Feature Reports Collection
	0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
	0x09, 0x10,        // Usage (0x10) - MDS
	0xA1, 0x01,        // Collection (Application)

	// Report ID 1: Supported Features (4 bytes)
	0x85, 0x01,        //   Report ID (1)
	0x09, 0x11,        //   Usage (0x11)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x04,        //   Report Count (4) - 32-bit features
	0xB1, 0x02,        //   Feature (Data,Var,Abs)

	// Report ID 2: Device Identifier (64 bytes max)
	0x85, 0x02,        //   Report ID (2)
	0x09, 0x12,        //   Usage (0x12)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x40,        //   Report Count (64)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)

	// Report ID 3: Data URI (128 bytes max)
	0x85, 0x03,        //   Report ID (3)
	0x09, 0x13,        //   Usage (0x13)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x80,        //   Report Count (128)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)

	// Report ID 4: Authorization (128 bytes max)
	0x85, 0x04,        //   Report ID (4)
	0x09, 0x14,        //   Usage (0x14)
	0x15, 0x00,        //   Logical Minimum (0)
	0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x80,        //   Report Count (128)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)

	// Report ID 5: Stream Control (1 byte)
	0x85, 0x05,        //   Report ID (5)
	0x09, 0x15,        //   Usage (0x15)
	0x15, 0x00,        //   Logical Minimum (0)
	0x25, 0x01,        //   Logical Maximum (1)
	0x75, 0x08,        //   Report Size (8)
	0x95, 0x01,        //   Report Count (1)
	0xB1, 0x02,        //   Feature (Data,Var,Abs)

	0xC0,              // End Collection
};

enum kb_leds_idx {
	KB_LED_NUMLOCK = 0,
	KB_LED_CAPSLOCK,
	KB_LED_SCROLLLOCK,
	KB_LED_COUNT,
};

static const struct gpio_dt_spec kb_leds[KB_LED_COUNT] = {
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0}),
};

enum kb_report_idx {
	KB_MOD_KEY = 0,
	KB_RESERVED,
	KB_KEY_CODE1,
	KB_KEY_CODE2,
	KB_KEY_CODE3,
	KB_KEY_CODE4,
	KB_KEY_CODE5,
	KB_KEY_CODE6,
	KB_REPORT_COUNT,
};

struct kb_event {
	uint16_t code;
	int32_t value;
};

K_MSGQ_DEFINE(kb_msgq, sizeof(struct kb_event), 2, 1);

UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
static uint32_t kb_duration;
static bool kb_ready;

static void input_cb(struct input_event *evt, void *user_data)
{
	struct kb_event kb_evt;

	ARG_UNUSED(user_data);

	kb_evt.code = evt->code;
	kb_evt.value = evt->value;
	if (k_msgq_put(&kb_msgq, &kb_evt, K_NO_WAIT) != 0) {
		LOG_ERR("Failed to put new input event");
	}
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void kb_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID device %s interface is %s",
		dev->name, ready ? "ready" : "not ready");
	kb_ready = ready;
}

static int kb_get_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 uint8_t *const buf)
{
	if (type == HID_REPORT_TYPE_FEATURE) {
		LOG_INF("Get Feature Report: ID %u, len %u", id, len);
		buf[0] = id; /* First byte is report ID */
		uint8_t *payload_buf = &buf[1];
		uint16_t payload_len = len - 1;

		switch (id) {
		case MDS_REPORT_ID_SUPPORTED_FEATURES:
			if (payload_len >= 4) {
				/* Return supported features as little-endian 32-bit */
				payload_buf[0] = (mds_supported_features >> 0) & 0xFF;
				payload_buf[1] = (mds_supported_features >> 8) & 0xFF;
				payload_buf[2] = (mds_supported_features >> 16) & 0xFF;
				payload_buf[3] = (mds_supported_features >> 24) & 0xFF;
				LOG_INF("Returning supported features: 0x%08X", mds_supported_features);
				return 4;
			}
			break;

		case MDS_REPORT_ID_DEVICE_IDENTIFIER:
			if (payload_len >= sizeof(mds_device_id)) {
				memcpy(payload_buf, mds_device_id, sizeof(mds_device_id));
				LOG_INF("Returning device ID: %s", mds_device_id);
				return sizeof(mds_device_id);
			}
			break;

		case MDS_REPORT_ID_DATA_URI:
			if (payload_len >= sizeof(mds_data_uri)) {
				memcpy(payload_buf, mds_data_uri, sizeof(mds_data_uri));
				LOG_INF("Returning data URI: %s", mds_data_uri);
				return sizeof(mds_data_uri);
			}
			break;

		case MDS_REPORT_ID_AUTHORIZATION:
			if (payload_len >= sizeof(mds_auth_token)) {
				memcpy(payload_buf, mds_auth_token, sizeof(mds_auth_token));
				LOG_INF("Returning auth token (length: %zu)", sizeof(mds_auth_token));
				return sizeof(mds_auth_token);
			}
			break;

		case MDS_REPORT_ID_STREAM_CONTROL:
			if (payload_len >= 1) {
				payload_buf[0] = mds_streaming_enabled ? MDS_STREAM_MODE_ENABLED : MDS_STREAM_MODE_DISABLED;
				LOG_INF("Returning stream control: %u", payload_buf[0]);
				return 1;
			}
			break;

		default:
			LOG_WRN("Unsupported feature report ID: %u", id);
			return -ENOTSUP;
		}

		LOG_ERR("Buffer too small for feature report ID %u (need %u, got %u)", id, len, len);
		return -EINVAL;
	}

	LOG_WRN("Get Report not implemented for type %u, ID %u", type, id);
	return -ENOTSUP;
}

static int kb_set_report(const struct device *dev,
			 const uint8_t type, const uint8_t id, const uint16_t len,
			 const uint8_t *const buf)
{
	if (type == HID_REPORT_TYPE_FEATURE) {
		LOG_INF("Set Feature Report: ID %u, len %u", id, len);
		LOG_HEXDUMP_INF(buf, len, "Feature data:");

		switch (id) {
		case MDS_REPORT_ID_STREAM_CONTROL:
			if (len >= 1) {
				bool enable = (buf[1] == MDS_STREAM_MODE_ENABLED);
				mds_streaming_enabled = enable;
				LOG_INF("Stream control: %s", enable ? "ENABLED" : "DISABLED");
				return 0;
			}
			LOG_ERR("Stream control report too short: %u bytes", len);
			return -EINVAL;

		default:
			LOG_WRN("Unsupported feature report ID for set: %u", id);
			return -ENOTSUP;
		}
	}

	if (type != HID_REPORT_TYPE_OUTPUT) {
		LOG_WRN("Unsupported report type for set: %u", type);
		return -ENOTSUP;
	}

	/* Handle keyboard LED output reports */
	for (unsigned int i = 0; i < ARRAY_SIZE(kb_leds); i++) {
		if (kb_leds[i].port == NULL) {
			continue;
		}

		(void)gpio_pin_set_dt(&kb_leds[i], buf[0] & BIT(i));
	}

	return 0;
}

/* Idle duration is stored but not used to calculate idle reports. */
static void kb_set_idle(const struct device *dev,
			const uint8_t id, const uint32_t duration)
{
	LOG_INF("Set Idle %u to %u", id, duration);
	kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id)
{
	LOG_INF("Get Idle %u to %u", id, kb_duration);
	return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto)
{
	LOG_INF("Protocol changed to %s",
		proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len,
			     const uint8_t *const buf)
{
	LOG_HEXDUMP_DBG(buf, len, "o.r.");

	// Check if this is a report with report ID
	if (len > 0) {
		uint8_t report_id = buf[0];

		switch (report_id) {
		case MDS_REPORT_ID_STREAM_DATA:
			/* MDS Stream Data Report */
			LOG_INF("Received MDS stream data report, len=%u", len);
			if (len > 1) {
				/* First byte after report ID is sequence number */
				uint8_t sequence = buf[1];
				LOG_INF("Stream packet sequence: %u", sequence);
				LOG_HEXDUMP_INF(&buf[2], len-2, "Stream payload:");

				/* Here you could process the stream data according to MDS protocol */
				if (mds_streaming_enabled) {
					LOG_INF("Processing stream data (streaming enabled)");
				} else {
					LOG_WRN("Received stream data but streaming is disabled");
				}
			}
			return;

		case 0:
			/* Standard keyboard LED output report (no report ID) */
			LOG_DBG("Keyboard LED report");
			break;

		default:
			LOG_INF("Received unknown output report ID: %u", report_id);
			return;
		}
	}

	// Handle as standard keyboard LED output report (no report ID)
	if (buf[0] == 0) {
		kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
	}
}

struct hid_device_ops kb_ops = {
	.iface_ready = kb_iface_ready,
	.get_report = kb_get_report,
	.set_report = kb_set_report,
	.set_idle = kb_set_idle,
	.get_idle = kb_get_idle,
	.set_protocol = kb_set_protocol,
	.output_report = kb_output_report,
};

/* doc device msg-cb start */
static void msg_cb(struct usbd_context *const usbd_ctx,
		   const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (msg->type == USBD_MSG_CONFIGURATION) {
		LOG_INF("\tConfiguration value %d", msg->status);
	}

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(usbd_ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(usbd_ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}
}
/* doc device msg-cb end */

int main(void)
{
	struct usbd_context *sample_usbd;
	const struct device *hid_dev;
	int ret;

	for (unsigned int i = 0; i < ARRAY_SIZE(kb_leds); i++) {
		if (kb_leds[i].port == NULL) {
			continue;
		}

		if (!gpio_is_ready_dt(&kb_leds[i])) {
			LOG_ERR("LED device %s is not ready", kb_leds[i].port->name);
			return -EIO;
		}

		ret = gpio_pin_configure_dt(&kb_leds[i], GPIO_OUTPUT_INACTIVE);
		if (ret != 0) {
			LOG_ERR("Failed to configure the LED pin, %d", ret);
			return -EIO;
		}
	}

	hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
	if (!device_is_ready(hid_dev)) {
		LOG_ERR("HID Device is not ready");
		return -EIO;
	}

	ret = hid_device_register(hid_dev,
				  hid_report_desc, sizeof(hid_report_desc),
				  &kb_ops);
	if (ret != 0) {
		LOG_ERR("Failed to register HID Device, %d", ret);
		return ret;
	}

	sample_usbd = sample_usbd_init_device(msg_cb);
	if (sample_usbd == NULL) {
		LOG_ERR("Failed to initialize USB device");
		return -ENODEV;
	}

	if (!usbd_can_detect_vbus(sample_usbd)) {
		/* doc device enable start */
		ret = usbd_enable(sample_usbd);
		if (ret) {
			LOG_ERR("Failed to enable device support");
			return ret;
		}
		/* doc device enable end */
	}

	LOG_INF("HID keyboard sample is initialized");

	while (true) {
		struct kb_event kb_evt;

		k_msgq_get(&kb_msgq, &kb_evt, K_FOREVER);

		switch (kb_evt.code) {
		case INPUT_KEY_0:
			if (kb_evt.value) {
				report[KB_KEY_CODE1] = HID_KEY_NUMLOCK;
			} else {
				report[KB_KEY_CODE1] = 0;
			}

			break;
		case INPUT_KEY_1:
			if (kb_evt.value) {
				report[KB_KEY_CODE2] = HID_KEY_CAPSLOCK;
			} else {
				report[KB_KEY_CODE2] = 0;
			}

			break;
		case INPUT_KEY_2:
			if (kb_evt.value) {
				report[KB_KEY_CODE3] = HID_KEY_SCROLLLOCK;
			} else {
				report[KB_KEY_CODE3] = 0;
			}

			break;
		case INPUT_KEY_3:
			if (kb_evt.value) {
				report[KB_MOD_KEY] = HID_KBD_MODIFIER_RIGHT_ALT;
				report[KB_KEY_CODE4] = HID_KEY_1;
				report[KB_KEY_CODE5] = HID_KEY_2;
				report[KB_KEY_CODE6] = HID_KEY_3;
			} else {
				report[KB_MOD_KEY] = HID_KBD_MODIFIER_NONE;
				report[KB_KEY_CODE4] = 0;
				report[KB_KEY_CODE5] = 0;
				report[KB_KEY_CODE6] = 0;
			}

			break;
		default:
			LOG_INF("Unrecognized input code %u value %d",
				kb_evt.code, kb_evt.value);
			continue;
		}

		if (!kb_ready) {
			LOG_INF("USB HID device is not ready");
			continue;
		}

		if (usbd_is_suspended(sample_usbd)) {
			/* on a press of any button, send wakeup request */
			if (kb_evt.value) {
				ret = usbd_wakeup_request(sample_usbd);
				if (ret) {
					LOG_ERR("Remote wakeup error, %d", ret);
				}
			}
			continue;
		}

		ret = hid_device_submit_report(hid_dev, KB_REPORT_COUNT, report);
		if (ret) {
			LOG_ERR("HID submit report error, %d", ret);
		}
	}

	return 0;
}
