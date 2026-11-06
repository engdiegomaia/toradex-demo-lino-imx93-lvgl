#include "home_header_layout.h"

void home_header_layout_compute(int name_w, int name_h, home_header_layout_t *out)
{
	int name_end, centre;

	if (!out)
		return;
	if (name_w < 0)
		name_w = 0;
	if (name_h < 0)
		name_h = 0;

	/* Keep the original spot unless a large board name would touch the box. */
	name_end = HOME_HEADER_NAME_X + name_w + HOME_HEADER_NAME_GAP;
	out->box_x = name_end > HOME_HEADER_GENERATED_BOX_X ? name_end
							    : HOME_HEADER_GENERATED_BOX_X;

	out->m33_icon_x = HOME_HEADER_A0_ICON_X + HOME_HEADER_ICON_W + HOME_HEADER_ICON_GAP;
	out->box_w = out->m33_icon_x + HOME_HEADER_ICON_W + HOME_HEADER_BOX_RIGHT_PAD;

	/* Centre the checkbox under the M33 icon. */
	out->radio_w = HOME_HEADER_RADIO_W;
	out->radio_x = out->m33_icon_x + HOME_HEADER_ICON_W / 2 - HOME_HEADER_RADIO_W / 2;
	out->m33_cb_x = 0;

	centre = HOME_HEADER_BOX_Y + HOME_HEADER_ICON_Y + HOME_HEADER_ICON_H / 2;
	out->name_y = centre - name_h / 2;
}
