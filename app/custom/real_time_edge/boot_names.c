#include "boot_names.h"

#define BOOT_TYPE_FIRST 1
#define BOOT_TYPE_LAST 8
#define APPS_PER_CORE 4

/* Short names: card title and log-frame event row (<= 23 characters). */
static const char *const s_app_name[APPS_PER_CORE] = {
	"FreeRTOS IRQ latency",
	"FreeRTOS RPMsg pingpong",
	"Zephyr IRQ latency",
	"Zephyr heartbeat",
};

/* Dropdown entries say what the application does, not its source folder. */
static const char *const s_menu_name[APPS_PER_CORE] = {
	"FreeRTOS: Interrupt latency benchmark",
	"FreeRTOS: RPMsg round-trip A55-M33",
	"Zephyr: Interrupt latency benchmark",
	"Zephyr: Heartbeat (tic/tac)",
};

static const char *const s_os_name[APPS_PER_CORE] = {
	"FreeRTOS", "FreeRTOS", "Zephyr", "Zephyr",
};

static int app_index(uint8_t boot_type)
{
	if (boot_type < BOOT_TYPE_FIRST || boot_type > BOOT_TYPE_LAST)
		return -1;
	return (boot_type - BOOT_TYPE_FIRST) % APPS_PER_CORE;
}

const char *boot_type_app_name(uint8_t boot_type)
{
	int i = app_index(boot_type);

	return i < 0 ? "No application" : s_app_name[i];
}

const char *boot_type_menu_name(uint8_t boot_type)
{
	int i = app_index(boot_type);

	return i < 0 ? "No application" : s_menu_name[i];
}

const char *boot_type_os_name(uint8_t boot_type)
{
	int i = app_index(boot_type);

	return i < 0 ? "RTOS" : s_os_name[i];
}
