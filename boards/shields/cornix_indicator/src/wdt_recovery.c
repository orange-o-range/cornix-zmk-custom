/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Watchdog auto-recovery for issue #14.
 *
 * On rare occasions the split peripheral's BLE link-layer controller hard-hangs
 * (an interrupt-locked spin in the radio ISR, with no fault and no assert) when
 * the central abruptly loses power -- and the half then stays dead until a manual
 * power cycle. Root-causing the exact controller line needs an SWD probe; this is
 * the robust mitigation: a hardware watchdog that resets the SoC if the firmware
 * ever stops being serviced, so the half reboots and re-establishes the split
 * link on its own within a few seconds instead of hanging forever.
 *
 * Confirmed against the diag build: with the assert handler disabled the right
 * half froze on central loss and the watchdog recovered it (next boot reported
 * reset_cause = WATCHDOG, "silent hard hang"). See issue #14.
 *
 * Feed: a k_timer, so the feed runs from the system-clock ISR and is immune to
 * false-fires from a merely busy work queue, yet still stops (and lets the WDT
 * fire) during the #14 hard hang, where interrupts themselves are locked.
 *
 * Power: this board does not use System OFF deep sleep (CONFIG_ZMK_SLEEP=n), so
 * the watchdog runs wall-clock and the feed simply piggybacks on the already
 * frequent BLE wakeups -- it adds no deep-sleep wake cost.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wdt_recovery, LOG_LEVEL_INF);

/* 6 s is far longer than any legitimate uninterrupted firmware operation, but
 * short enough that a #14 hang recovers quickly. Feed at a third of that. */
#define WDT_TIMEOUT_MS 6000
#define WDT_FEED_MS    (WDT_TIMEOUT_MS / 3)

static const struct device *const wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));
static int wdt_ch = -1;

static void wdt_feed_timer_fn(struct k_timer *timer) {
    ARG_UNUSED(timer);
    if (wdt_ch >= 0) {
        (void)wdt_feed(wdt_dev, wdt_ch);
    }
}
static K_TIMER_DEFINE(wdt_feed_timer, wdt_feed_timer_fn, NULL);

static int wdt_recovery_init(void) {
    if (!device_is_ready(wdt_dev)) {
        LOG_WRN("wdt0 not ready; #14 auto-recovery disabled");
        return 0;
    }
    const struct wdt_timeout_cfg cfg = {
        .flags = WDT_FLAG_RESET_SOC,
        .window = {.min = 0U, .max = WDT_TIMEOUT_MS},
        .callback = NULL,
    };
    wdt_ch = wdt_install_timeout(wdt_dev, &cfg);
    if (wdt_ch < 0) {
        LOG_ERR("wdt_install_timeout failed: %d", wdt_ch);
        return 0;
    }
    if (wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG) != 0) {
        LOG_ERR("wdt_setup failed");
        wdt_ch = -1;
        return 0;
    }
    k_timer_start(&wdt_feed_timer, K_MSEC(WDT_FEED_MS), K_MSEC(WDT_FEED_MS));
    return 0;
}
SYS_INIT(wdt_recovery_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
