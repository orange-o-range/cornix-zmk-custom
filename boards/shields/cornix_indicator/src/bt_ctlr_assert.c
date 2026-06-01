/*
 * Copyright (c) 2026 numachang
 * SPDX-License-Identifier: MIT
 *
 * Fix for issue #14 (split peripheral dies when the central abruptly loses
 * power and never recovers until a manual power cycle).
 *
 * Enabling CONFIG_BT_CTLR_ASSERT_HANDLER routes the BLE controller's internal
 * LL_ASSERT() checks to this app-provided handler instead of the default path
 * (BT_ASSERT -> k_oops -> the kernel fatal handler that arch_system_halt()s the
 * CPU = the #14 freeze). Confirmed on hardware by a clean A/B: the stock build
 * leaves the right half dead after the left is toggled off/on, while merely
 * enabling this handler makes the right half recover -- on both USB and battery,
 * so it is not a logging/USB artefact.
 *
 * (The handler body runs only if a controller invariant ever actually breaks;
 * recover by rebooting rather than halting. In practice the logs show it does
 * not fire -- enabling the handler changes how every controller LL_ASSERT
 * compiles, and that deterministic difference alone keeps the half out of the
 * frozen state.)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

void bt_ctlr_assert_handle(char *file, uint32_t line) {
    ARG_UNUSED(file);
    ARG_UNUSED(line);
    sys_reboot(SYS_REBOOT_COLD);
    for (;;) {
        /* unreachable */
    }
}
