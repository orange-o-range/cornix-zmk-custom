/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Stage B2 (central / left half):
 *   pixel[1] (outer): BT profile colour (BT0=green / BT1=red / BT2=blue)
 *                     on every active profile change.
 *   pixel[0] (inner): blue burst when the split peripheral connects /
 *                     disconnects, plus a red blink on self battery low.
 *
 * Continuous "searching" or "peer-lost" blinking comes in B3.
 * Charging colours come in B4 once the STAT GPIO is identified.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include <cornix_rgb_indicator/widget.h>

LOG_MODULE_DECLARE(cornix_rgb, CONFIG_ZMK_LOG_LEVEL);

#define BATTERY_LEVEL_LOW 20

static const struct led_rgb COL_RED   = {.r = CORNIX_RGB_LEVEL};
static const struct led_rgb COL_GREEN = {.g = CORNIX_RGB_LEVEL};
static const struct led_rgb COL_BLUE  = {.b = CORNIX_RGB_LEVEL};

static struct led_rgb bt_profile_color(uint8_t profile_index) {
    switch (profile_index) {
    case 0:
        return COL_GREEN;
    case 1:
        return COL_RED;
    case 2:
        return COL_BLUE;
    default:
        return COL_BLUE;
    }
}

static int on_bt_profile_changed(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    int profile = zmk_ble_active_profile_index();
    if (profile < 0) {
        return 0;
    }
    LOG_INF("central: BT profile -> %d", profile);
    cornix_rgb_show_once(CORNIX_RGB_PIX_OUTER,
                         bt_profile_color((uint8_t)profile), 600);
    return 0;
}

ZMK_LISTENER(cornix_rgb_central_bt, on_bt_profile_changed);
ZMK_SUBSCRIPTION(cornix_rgb_central_bt, zmk_ble_active_profile_changed);

static int on_peripheral_status(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (ev == NULL) {
        return 0;
    }
    LOG_INF("central: peripheral %s",
            ev->connected ? "connected" : "disconnected");
    if (ev->connected) {
        cornix_rgb_show_once(CORNIX_RGB_PIX_INNER, COL_BLUE, 600);
    } else {
        cornix_rgb_blink(CORNIX_RGB_PIX_INNER, COL_BLUE, 2, 400, 400);
    }
    return 0;
}

ZMK_LISTENER(cornix_rgb_central_peer, on_peripheral_status);
ZMK_SUBSCRIPTION(cornix_rgb_central_peer, zmk_split_peripheral_status_changed);

static int on_battery_changed(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev == NULL || ev->state_of_charge == 0) {
        return 0;
    }
    if (ev->state_of_charge <= BATTERY_LEVEL_LOW) {
        LOG_INF("central: battery low %u%%", ev->state_of_charge);
        cornix_rgb_blink(CORNIX_RGB_PIX_INNER, COL_RED, 2, 400, 400);
    }
    return 0;
}

ZMK_LISTENER(cornix_rgb_central_bat, on_battery_changed);
ZMK_SUBSCRIPTION(cornix_rgb_central_bat, zmk_battery_state_changed);

#endif /* CONFIG_ZMK_SPLIT_ROLE_CENTRAL */
