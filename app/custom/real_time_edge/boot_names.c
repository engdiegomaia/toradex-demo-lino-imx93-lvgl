#include "boot_names.h"

#define BOOT_TYPE_FIRST 1
#define BOOT_TYPE_LAST 8
#define APPS_PER_CORE 4

static const char *const s_app_name[APPS_PER_CORE] = {
	"FreeRTOS rt_latency",
	"FreeRTOS hello_world",
	"Zephyr rt_latency",
	"Zephyr hello_world",
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

const char *boot_type_os_name(uint8_t boot_type)
{
	int i = app_index(boot_type);

	return i < 0 ? "RTOS" : s_os_name[i];
}
