/*
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for the advertising set terminated callback added to
 * struct bt_le_ext_adv_cb (subsys/bluetooth/host/adv.c,
 * bt_hci_le_adv_set_terminated()).
 *
 * The tests drive the HCI event handler directly with crafted LE Advertising
 * Set Terminated events and observe what is delivered to the application
 * through the new bt_le_ext_adv_cb.terminated callback.
 */

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/kernel.h>
#include <zephyr/net_buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#include <host/hci_core.h>

/* adv_handle of the advertising set created with the terminated callback. */
#define ADV_HANDLE_WITH_CB    0
/* adv_handle of the advertising set created with no callbacks at all. */
#define ADV_HANDLE_WITHOUT_CB 1

NET_BUF_SIMPLE_DEFINE_STATIC(evt_buf, sizeof(struct bt_hci_evt_le_adv_set_terminated));
static struct net_buf evt_net_buf;

/* Captured state from the last terminated callback invocation. */
struct captured_terminated {
	bool called;
	unsigned int call_count;
	struct bt_le_ext_adv *adv;
	uint8_t reason;
	uint8_t num_completed_ext_adv_evts;
};

static struct captured_terminated last_terminated;

static struct bt_le_ext_adv *adv_with_cb;
static struct bt_le_ext_adv *adv_without_cb;

static void terminated_cb(struct bt_le_ext_adv *adv,
			   const struct bt_le_ext_adv_terminated_info *info)
{
	last_terminated.called = true;
	last_terminated.call_count++;
	last_terminated.adv = adv;
	last_terminated.reason = info->reason;
	last_terminated.num_completed_ext_adv_evts = info->num_completed_ext_adv_evts;
}

static const struct bt_le_ext_adv_cb adv_cb_with_terminated = {
	.terminated = terminated_cb,
};

/*
 * A registered callback struct with no terminated member set, to exercise
 * the adv->cb->terminated NULL-check without a NULL adv->cb.
 */
static const struct bt_le_ext_adv_cb adv_cb_without_terminated = {0};

static struct bt_le_ext_adv *create_adv_set(const struct bt_le_ext_adv_cb *cb)
{
	struct bt_le_ext_adv *adv;
	int err;

	err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, cb, &adv);

	zassert_ok(err, "Failed to create ext adv set (%d)", err);
	zassert_not_null(adv, "adv set is NULL");

	return adv;
}

/*
 * Build and deliver a full LE Advertising Set Terminated event to the
 * handler under test.
 */
static void deliver_terminated_event(uint8_t adv_handle, uint8_t status,
				      uint16_t conn_handle, uint8_t num_completed_ext_adv_evts)
{
	struct bt_hci_evt_le_adv_set_terminated evt = {
		.status = status,
		.adv_handle = adv_handle,
		.conn_handle = sys_cpu_to_le16(conn_handle),
		.num_completed_ext_adv_evts = num_completed_ext_adv_evts,
	};

	net_buf_simple_reset(&evt_buf);
	net_buf_simple_add_mem(&evt_buf, &evt, sizeof(evt));

	evt_net_buf.b = evt_buf;
	bt_hci_le_adv_set_terminated(&evt_net_buf);

	evt_buf = evt_net_buf.b;
}

static void *set_terminated_setup(void)
{
	/* Provide a valid identity so bt_le_ext_adv_create() accepts params. */
	memset(&bt_dev, 0, sizeof(bt_dev));
	bt_dev.id_count = 1;
	bt_dev.hci_version = BT_HCI_VERSION_5_0;
	/* A non-zero identity address is required; BT_ADDR_LE_ANY is rejected. */
	bt_dev.id_addr[0].type = BT_ADDR_LE_RANDOM;
	bt_dev.id_addr[0].a.val[0] = 0x01;
	bt_dev.id_addr[0].a.val[5] = 0xC0;
	atomic_set_bit(bt_dev.flags, BT_DEV_READY);

	adv_with_cb = create_adv_set(&adv_cb_with_terminated);
	adv_without_cb = create_adv_set(&adv_cb_without_terminated);

	zassert_equal(bt_le_ext_adv_get_index(adv_with_cb), ADV_HANDLE_WITH_CB,
		      "Unexpected adv_handle for the callback-bearing set");
	zassert_equal(bt_le_ext_adv_get_index(adv_without_cb), ADV_HANDLE_WITHOUT_CB,
		      "Unexpected adv_handle for the callback-less set");

	return NULL;
}

static void set_terminated_before(void *fixture)
{
	ARG_UNUSED(fixture);

	memset(&last_terminated, 0, sizeof(last_terminated));
}

ZTEST_SUITE(bt_adv_set_terminated, NULL, set_terminated_setup, set_terminated_before, NULL, NULL);

/*
 * A terminated event caused by the duration/max-events limit being reached
 * must invoke the terminated callback with the reason and event count
 * carried verbatim from the HCI event.
 */
ZTEST(bt_adv_set_terminated, test_terminated_reports_limit_reached)
{
	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_LIMIT_REACHED, 0, 42);

	zassert_equal(last_terminated.call_count, 1, "Expected exactly one callback invocation");
	zassert_true(last_terminated.called, "Callback not invoked");
	zassert_equal(last_terminated.adv, adv_with_cb, "Wrong adv set passed to callback");
	zassert_equal(last_terminated.reason, BT_HCI_ERR_LIMIT_REACHED, "Wrong reason");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 42, "Wrong event count");
}

/*
 * BT_HCI_ERR_ADV_TIMEOUT (duration elapsed) must also be reported verbatim.
 */
ZTEST(bt_adv_set_terminated, test_terminated_reports_adv_timeout)
{
	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_ADV_TIMEOUT, 0, 7);

	zassert_equal(last_terminated.call_count, 1, "Expected exactly one callback invocation");
	zassert_equal(last_terminated.reason, BT_HCI_ERR_ADV_TIMEOUT, "Wrong reason");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 7, "Wrong event count");
}

/*
 * A connection created (status success) must also invoke the terminated
 * callback, in addition to whatever else the handler does for the new
 * connection.
 */
ZTEST(bt_adv_set_terminated, test_terminated_reports_connection_created)
{
	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_SUCCESS, 0x0001, 3);

	zassert_equal(last_terminated.call_count, 1, "Expected exactly one callback invocation");
	zassert_equal(last_terminated.reason, BT_HCI_ERR_SUCCESS, "Wrong reason");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 3, "Wrong event count");
}

/*
 * A Controller error status must be reported verbatim as well; the callback
 * is invoked regardless of which status code the Controller reports.
 */
ZTEST(bt_adv_set_terminated, test_terminated_reports_controller_error)
{
	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_UNSPECIFIED, 0, 0);

	zassert_equal(last_terminated.call_count, 1, "Expected exactly one callback invocation");
	zassert_equal(last_terminated.reason, BT_HCI_ERR_UNSPECIFIED, "Wrong reason");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 0, "Wrong event count");
}

/*
 * Regression/safety test: when the registered callback struct does not set
 * .terminated, the adv->cb->terminated NULL-check must prevent a crash and
 * simply skip the callback.
 */
ZTEST(bt_adv_set_terminated, test_no_crash_when_terminated_not_registered)
{
	deliver_terminated_event(ADV_HANDLE_WITHOUT_CB, BT_HCI_ERR_LIMIT_REACHED, 0, 1);

	zassert_equal(last_terminated.call_count, 0,
		      "Callback must not be invoked for a set without .terminated");
}

/*
 * Two consecutive terminated events for the same set must each independently
 * invoke the callback once, with their own reason/count.
 */
ZTEST(bt_adv_set_terminated, test_terminated_reports_each_event_independently)
{
	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_LIMIT_REACHED, 0, 10);
	zassert_equal(last_terminated.call_count, 1, "Expected exactly one callback invocation");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 10, "Wrong event count");

	deliver_terminated_event(ADV_HANDLE_WITH_CB, BT_HCI_ERR_ADV_TIMEOUT, 0, 20);
	zassert_equal(last_terminated.call_count, 2, "Expected a second callback invocation");
	zassert_equal(last_terminated.num_completed_ext_adv_evts, 20, "Wrong event count");
}
