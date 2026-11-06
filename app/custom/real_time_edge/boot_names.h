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

/* Short name for the card and the log frame, e.g. "FreeRTOS IRQ latency".
 * At most 23 characters so it fits the frame's event row. "No application"
 * for an invalid boot type. */
const char *boot_type_app_name(uint8_t boot_type);

/* Descriptive name for the application dropdown, e.g.
 * "FreeRTOS: Interrupt latency benchmark". "No application" if invalid. */
const char *boot_type_menu_name(uint8_t boot_type);

/* "FreeRTOS" or "Zephyr"; "RTOS" for an invalid boot type. */
const char *boot_type_os_name(uint8_t boot_type);

#endif /* BOOT_NAMES_H */
