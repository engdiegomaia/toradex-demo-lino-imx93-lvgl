/*
 * boot_names - human-readable names for the boot types in basedef.h.
 *
 * Boot types 1..4 are Cortex-A and 5..8 Cortex-M, each with the same four
 * applications in this order: FreeRTOS rt_latency, FreeRTOS hello_world,
 * Zephyr rt_latency, Zephyr hello_world. No LVGL dependency (host-testable).
 */
#ifndef BOOT_NAMES_H
#define BOOT_NAMES_H

#include <stdint.h>

/* e.g. "FreeRTOS rt_latency"; at most 23 characters so it fits the on-screen
 * frame's event row. "No application" for an invalid boot type. */
const char *boot_type_app_name(uint8_t boot_type);

/* "FreeRTOS" or "Zephyr"; "RTOS" for an invalid boot type. */
const char *boot_type_os_name(uint8_t boot_type);

#endif /* BOOT_NAMES_H */
