/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Stage B2 (peripheral / right half):
 *   pixel[0] (inner): red blink on self battery low.
 *   pixel[1] (outer): blue burst when the link to the central comes up
 *                     or goes down.
 *
 * Continuous "central-lost" blinking comes in B3.
 * Charging colours come in B4.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include <cornix_rgb_indicator/widget.h>

LOG_MODULE_DECLARE(cornix_rgb, CONFIG_ZMK_LOG_LEVEL);

#define BATTERY_LEVEL_LOW 20

static const struct led_rgb COL_RED  = {.r = CORNIX_RGB_LEVEL};
static const struct led_rgb COL_BLUE = {.b = CORNIX_RGB_LEVEL};

static int on_central_status(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);
    if (ev == NULL) {
        return 0;
    }
    LOG_INF("peripheral: central %s",
            ev->connected ? "connected" : "disconnected");
    if (ev->connected) {
        cornix_rgb_show_once(CORNIX_RGB_PIX_OUTER, COL_BLUE, 3000);
    } else {
        cornix_rgb_blink(CORNIX_RGB_PIX_OUTER, COL_BLUE, 2, 400, 400);
    }
    return 0;
}

ZMK_LISTENER(cornix_rgb_periph_peer, on_central_status);
ZMK_SUBSCRIPTION(cornix_rgb_periph_peer, zmk_split_peripheral_status_changed);

static int on_battery_changed(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    if (ev == NULL || ev->state_of_charge == 0) {
        return 0;
    }
    if (ev->state_of_charge <= BATTERY_LEVEL_LOW) {
        LOG_INF("peripheral: battery low %u%%", ev->state_of_charge);
        cornix_rgb_blink(CORNIX_RGB_PIX_INNER, COL_RED, 2, 400, 400);
    }
    return 0;
}

ZMK_LISTENER(cornix_rgb_periph_bat, on_battery_changed);
ZMK_SUBSCRIPTION(cornix_rgb_periph_bat, zmk_battery_state_changed);

#endif /* CONFIG_ZMK_SPLIT && !CONFIG_ZMK_SPLIT_ROLE_CENTRAL */
