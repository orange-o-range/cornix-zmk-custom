/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Stage B2: shared LED strip worker.
 *
 * Owns the led_strip device and the only thread that touches it. Pulls
 * cornix_rgb_msg off a message queue and renders the requested blink on
 * the requested pixel. Per-half listeners (central.c / peripheral.c)
 * translate ZMK events into messages and enqueue them.
 *
 * EXT_POWER is enabled once at boot and left on for B2. Power-gating it
 * around idle periods is B5.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/ext_power.h>

#include <cornix_rgb_indicator/widget.h>

LOG_MODULE_REGISTER(cornix_rgb, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_NODE       DT_ALIAS(status_ws2812)
#define STRIP_NUM_PIXELS DT_PROP(STRIP_NODE, chain_length)

BUILD_ASSERT(DT_NODE_HAS_STATUS(STRIP_NODE, okay),
             "cornix_rgb: status-ws2812 alias must point at an okay node");

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);

#if DT_HAS_COMPAT_STATUS_OKAY(zmk_ext_power_generic)
#define HAVE_EXT_POWER 1
static const struct device *const ext_power =
    DEVICE_DT_GET(DT_INST(0, zmk_ext_power_generic));
#else
#define HAVE_EXT_POWER 0
#endif

K_MSGQ_DEFINE(cornix_rgb_msgq, sizeof(struct cornix_rgb_msg), 16, 4);

static struct led_rgb pixels[STRIP_NUM_PIXELS];

void cornix_rgb_queue(const struct cornix_rgb_msg *msg) {
    if (k_msgq_put(&cornix_rgb_msgq, msg, K_NO_WAIT) != 0) {
        LOG_WRN("cornix_rgb: msgq full, dropping msg for pixel %u",
                msg->pixel_index);
    }
}

void cornix_rgb_show_once(uint8_t idx, struct led_rgb color, uint16_t on_ms) {
    struct cornix_rgb_msg msg = {
        .pixel_index = idx,
        .color = color,
        .blink_count = 1,
        .on_ms = on_ms,
        .off_ms = 0,
    };
    cornix_rgb_queue(&msg);
}

void cornix_rgb_blink(uint8_t idx, struct led_rgb color, uint8_t count,
                      uint16_t on_ms, uint16_t off_ms) {
    struct cornix_rgb_msg msg = {
        .pixel_index = idx,
        .color = color,
        .blink_count = count ? count : 1,
        .on_ms = on_ms,
        .off_ms = off_ms,
    };
    cornix_rgb_queue(&msg);
}

static void write_pixel(uint8_t idx, struct led_rgb color) {
    if (idx >= STRIP_NUM_PIXELS) {
        return;
    }
    pixels[idx] = color;
    led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
}

static void cornix_rgb_thread_fn(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(strip)) {
        LOG_ERR("cornix_rgb: led_strip device not ready");
        return;
    }
#if HAVE_EXT_POWER
    if (device_is_ready(ext_power)) {
        ext_power_enable(ext_power);
    } else {
        LOG_WRN("cornix_rgb: ext_power device not ready");
    }
#endif

    LOG_INF("cornix_rgb worker started, %d pixel(s)", STRIP_NUM_PIXELS);

    while (true) {
        struct cornix_rgb_msg msg;
        k_msgq_get(&cornix_rgb_msgq, &msg, K_FOREVER);

        for (uint8_t i = 0; i < msg.blink_count; i++) {
            write_pixel(msg.pixel_index, msg.color);
            k_sleep(K_MSEC(msg.on_ms));

            write_pixel(msg.pixel_index, (struct led_rgb){0});
            if (i + 1 < msg.blink_count) {
                k_sleep(K_MSEC(msg.off_ms));
            }
        }
    }
}

K_THREAD_DEFINE(cornix_rgb_tid, 1024, cornix_rgb_thread_fn,
                NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 200);
