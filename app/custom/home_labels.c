/*
 * Copyright 2026 Toradex
 */
#include "home_labels.h"

#include "lvgl.h"

#define BOARD_NAME "Lino i.MX93"
#define M33_TITLE  "Cortex-M33"

void home_labels_init(gg_ui_t * ui)
{
    if(ui == NULL) return;

    if(ui->Home.board_name != NULL)
        lv_label_set_text(ui->Home.board_name, BOARD_NAME);
    if(ui->Home.container_cortexM33_title_coreM33 != NULL)
        lv_label_set_text(ui->Home.container_cortexM33_title_coreM33, M33_TITLE);
}
