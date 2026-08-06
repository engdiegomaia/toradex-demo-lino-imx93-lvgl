/*
* Copyright 2024-2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

/*********************
 *      INCLUDES
 *********************/
#define _DEFAULT_SOURCE /* needed for usleep() */
#include <stdlib.h>
#include <unistd.h>
#define SDL_MAIN_HANDLED        /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL2/SDL.h>
#include "lvgl.h"
#include "gui_guider.h"
#include "gg_utils.h"
#include "custom.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/*********************
 *      DEFINES
 *********************/
#ifndef GG_USE_KEYBOARD
#define GG_USE_KEYBOARD 0
#endif

#ifndef GG_SIMULATOR_ZOOM
#define GG_SIMULATOR_ZOOM 1
#endif

#define GG_UNUSED(x) ((void)x)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t * hal_init(int32_t w, int32_t h);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *      VARIABLES
 **********************/
extern uint8_t simulator_icon[];
gg_ui_t guider_ui;

int main(int argc, char **argv)
{
    GG_UNUSED(argc);
    GG_UNUSED(argv);

    /*Initialize LVGL*/
    lv_init();

    /*Initialize the display and input devices*/
    hal_init(LV_HOR_RES_MAX, LV_VER_RES_MAX);

    setup_ui(&guider_ui);
    custom_init(&guider_ui);

    while(1) {
        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.*/
        lv_timer_handler();
        usleep(5 * 1000);
    }

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the LVGL graphics
 * library
 */
static lv_display_t * hal_init(int32_t w, int32_t h)
{
    lv_display_t * disp = lv_sdl_window_create(w, h);
    lv_sdl_window_set_title(disp, "Simulator (C/C++)");
    lv_sdl_window_set_icon(disp, simulator_icon, 32, 32);
    lv_sdl_window_set_resizeable(disp, false);
    lv_sdl_window_set_zoom(disp, GG_SIMULATOR_ZOOM);

    lv_indev_t * mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);
    lv_display_set_default(disp);

#if GG_USE_KEYBOARD
    lv_group_set_default(lv_group_create());
    lv_indev_t * keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, disp);
    lv_indev_set_group(keyboard, lv_group_get_default());
#endif

    return disp;
}
