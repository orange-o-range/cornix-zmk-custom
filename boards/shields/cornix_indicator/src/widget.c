/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Stage B1: minimal boot-time blink to verify WS2812 strip + EXT_POWER
 * wiring on both halves. LED0 = dim red, LED1 = dim green, 200 ms, then off.
 * Per-half / event-driven behaviour comes in B2+.
 */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/ext_power.h>

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

static struct led_rgb pixels[STRIP_NUM_PIXELS];

static void cornix_rgb_boot_blink(void) {
#if HAVE_EXT_POWER
    if (device_is_ready(ext_power)) {
        ext_power_enable(ext_power);
    }
#endif

    pixels[0] = (struct led_rgb){.r = 0x10};
    if (STRIP_NUM_PIXELS > 1) {
        pixels[1] = (struct led_rgb){.g = 0x10};
    }
    int rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    LOG_INF("cornix_rgb boot blink on (rc=%d)", rc);

    k_sleep(K_MSEC(200));

    memset(pixels, 0, sizeof(pixels));
    rc = led_strip_update_rgb(strip, pixels, STRIP_NUM_PIXELS);
    LOG_INF("cornix_rgb boot blink off (rc=%d)", rc);

#if HAVE_EXT_POWER
    if (device_is_ready(ext_power)) {
        ext_power_disable(ext_power);
    }
#endif
}

static void cornix_rgb_thread_fn(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    if (!device_is_ready(strip)) {
        LOG_ERR("cornix_rgb: led_strip device not ready");
        return;
    }

    k_sleep(K_MSEC(500));
    cornix_rgb_boot_blink();
}

K_THREAD_DEFINE(cornix_rgb_tid, 1024, cornix_rgb_thread_fn,
                NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
