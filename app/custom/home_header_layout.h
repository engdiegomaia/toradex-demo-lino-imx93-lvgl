/*
 * home_header_layout - geometry of the Home header (board name + core selector).
 *
 * The generated screen spreads the Cortex-A55 and Cortex-M33 icons ~300 px
 * apart inside a 609 px box, and sets the board name in the default 14 px
 * font. With the A55-1 lane retired that leaves a large empty gap. This
 * computes a tighter layout; home_labels.c applies it at runtime so it
 * survives GUI Guider regeneration. Pure arithmetic, host-tested.
 */
#ifndef HOME_HEADER_LAYOUT_H
#define HOME_HEADER_LAYOUT_H

/* Values from gg_Home.c, the layout being replaced. */
#define HOME_HEADER_GENERATED_BOX_X 149
#define HOME_HEADER_GENERATED_BOX_W 609
#define HOME_HEADER_BOX_Y 63
#define HOME_HEADER_A0_ICON_X 65
#define HOME_HEADER_ICON_Y 17
#define HOME_HEADER_ICON_W 80
#define HOME_HEADER_ICON_H 84
#define HOME_HEADER_NAME_X 16
#define HOME_HEADER_NAME_GAP 16 /* between the board name and the box */

#define HOME_HEADER_ICON_GAP 70 /* between the two core icons */
#define HOME_HEADER_BOX_RIGHT_PAD 30
#define HOME_HEADER_RADIO_W 130 /* holds the "Cortex-M33" checkbox */

typedef struct {
	int box_x, box_w;        /* container_1 */
	int m33_icon_x;          /* inside the box */
	int radio_x, radio_w;    /* container_1_container_radio, inside the box */
	int m33_cb_x;            /* checkbox inside the radio container */
	int name_y;              /* board name, screen coordinates */
} home_header_layout_t;

/* name_w/name_h are the rendered size of the board name in pixels.
 * align_right_px > 0 puts the box's right edge there (the dropdown's right
 * edge), as long as the box still clears the board name; <= 0 keeps the
 * generated left position. */
void home_header_layout_compute(int name_w, int name_h, int align_right_px,
				home_header_layout_t *out);

#endif
