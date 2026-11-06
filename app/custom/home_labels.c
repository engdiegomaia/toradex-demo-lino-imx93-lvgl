/*
 * Copyright 2026 Toradex
 */
#include "home_labels.h"

#include "lvgl.h"
#include "home_header_layout.h"

#define BOARD_NAME "Lino i.MX93"
#define M33_TITLE  "Cortex-M33"

#define BOARD_NAME_FONT (&lv_font_montserratMedium_28)

/* Larger board name and a tighter core-selector box. The generated layout puts
 * the two core icons ~300 px apart; see home_header_layout.h. */
static void apply_header_layout(gg_ui_t * ui)
{
    gg_Home_t * h = &ui->Home;
    home_header_layout_t l;
    lv_point_t size;
    int right_edge;

    if(h->board_name == NULL || h->container_1 == NULL) return;

    lv_obj_set_style_text_font(h->board_name, BOARD_NAME_FONT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_text_get_size(&size, BOARD_NAME, BOARD_NAME_FONT, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    /* Right-align the box with the app dropdown below it. */
    right_edge = h->ddlist_apps ? lv_obj_get_x(h->ddlist_apps) + lv_obj_get_width(h->ddlist_apps) : 0;
    home_header_layout_compute(size.x, size.y, right_edge, &l);

    lv_obj_set_y(h->board_name, l.name_y);
    lv_obj_set_x(h->container_1, l.box_x);
    lv_obj_set_width(h->container_1, l.box_w);

    if(h->container_1_core_m33_icon) lv_obj_set_x(h->container_1_core_m33_icon, l.m33_icon_x);
    if(h->container_1_container_radio) {
        lv_obj_set_x(h->container_1_container_radio, l.radio_x);
        lv_obj_set_width(h->container_1_container_radio, l.radio_w);
    }
    if(h->container_radio_cb_core_m33) lv_obj_set_x(h->container_radio_cb_core_m33, l.m33_cb_x);
}

void home_labels_init(gg_ui_t * ui)
{
    if(ui == NULL) return;

    if(ui->Home.board_name != NULL)
        lv_label_set_text(ui->Home.board_name, BOARD_NAME);
    if(ui->Home.container_cortexM33_title_coreM33 != NULL)
        lv_label_set_text(ui->Home.container_cortexM33_title_coreM33, M33_TITLE);

    apply_header_layout(ui);
}
