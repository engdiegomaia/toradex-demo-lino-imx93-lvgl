/*
 * Copyright 2026 Toradex
 *
 * Home screen branding quadrant. See branding.h for the contract.
 */

#include "branding.h"

#include "lvgl.h"

/* All three emitted by scripts/png-to-lvgl-image.py; see docs/DEVELOPMENT.md for the
 * commands. GUI-Guider does ship Torizon and Toradex lockups in gg_image.h, but
 * only at 188x72 and 164x42 -- these are the same artwork 15% larger, redrawn
 * from the high-res originals so the extra size buys sharpness rather than
 * upsampling blur. The LVGL lockup has no generated counterpart at all. */
LV_IMAGE_DECLARE(torizon_os_227x83_RGB565A8);
LV_IMAGE_DECLARE(lvgl_logo_179x68_RGB565A8);
LV_IMAGE_DECLARE(toradex_189x48_RGB565A8);

/* Same caption blue the readouts use, so the quadrant title matches its
 * neighbours rather than introducing a third heading colour. */
#define COLOR_TITLE 0x014B88

/* container_cortexA0 is 264 x 493 at (5, 297) with zero padding. The three
 * lockups are 83 + 68 + 48 px tall; spreading the 447 px below the title over
 * four equal 62 px gaps puts them here. Positions are TOP_MID-relative so a
 * width change re-centres them instead of shifting them off the panel -- which
 * matters for the 227 px Torizon lockup, the widest of the three. */
#define BRAND_TITLE_X 3
#define BRAND_TITLE_Y 16
#define BRAND_Y_TORIZON 108
#define BRAND_Y_LVGL    253
#define BRAND_Y_TORADEX 383

static lv_obj_t * add_logo(lv_obj_t * parent, const lv_image_dsc_t * src, int y)
{
    lv_obj_t * img = lv_image_create(parent);

    lv_obj_set_size(img, src->header.w, src->header.h);
    lv_obj_set_align(img, LV_ALIGN_TOP_MID);
    lv_obj_set_x(img, 0);
    lv_obj_set_y(img, y);
    lv_image_set_src(img, src);

    return img;
}

void branding_build_home(gg_ui_t * ui)
{
    lv_obj_t * panel;
    lv_obj_t * title;
    uint32_t i;
    uint32_t count;

    if (!ui || !ui->Home.container_cortexA0)
        return;

    panel = ui->Home.container_cortexA0;

    /* Hide the whole generated payload rather than naming the nine spec labels
     * one by one: the panel's entire contents are superseded, and iterating
     * keeps working if GUI-Guider adds or removes a row. The Torizon lockup is
     * among them and is re-created below at a position we control. */
    count = lv_obj_get_child_count(panel);
    for (i = 0; i < count; i++)
        lv_obj_add_flag(lv_obj_get_child(panel, i), LV_OBJ_FLAG_HIDDEN);

    title = lv_label_create(panel);
    lv_obj_set_width(title, LV_SIZE_CONTENT);
    lv_obj_set_height(title, LV_SIZE_CONTENT);
    lv_obj_set_align(title, LV_ALIGN_TOP_LEFT);
    lv_obj_set_x(title, BRAND_TITLE_X);
    lv_obj_set_y(title, BRAND_TITLE_Y);
    lv_label_set_text(title, "Powered by");
    lv_obj_set_style_text_font(title, &lv_font_montserratMedium_20,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_TITLE),
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    add_logo(panel, &torizon_os_227x83_RGB565A8, BRAND_Y_TORIZON);
    add_logo(panel, &lvgl_logo_179x68_RGB565A8, BRAND_Y_LVGL);
    add_logo(panel, &toradex_189x48_RGB565A8, BRAND_Y_TORADEX);

    /* Home also drops a loose Toradex lockup on the screen at (16, 721), which
     * lands inside this panel and would double up with the one above. The
     * quadrant owns the branding now, so the loose copy goes. */
    if (ui->Home.image_1)
        lv_obj_add_flag(ui->Home.image_1, LV_OBJ_FLAG_HIDDEN);
}
