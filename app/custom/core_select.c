#include "core_select.h"

static int is_core(int v)
{
	return v == CORE_SELECT_A55 || v == CORE_SELECT_M33;
}

int core_select_toggle(int current, int clicked)
{
	if (!is_core(clicked))
		return current;
	return clicked == current ? CORE_SELECT_NONE : clicked;
}

int core_select_can_operate(int selection)
{
	return is_core(selection);
}
