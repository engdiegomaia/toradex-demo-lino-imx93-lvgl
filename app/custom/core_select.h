/*
 * core_select - which core the Home START/STOP buttons act on.
 *
 * The checkbox pair used to behave as a radio group (one core always
 * selected). Clicking the checked box now clears the selection; with nothing
 * selected START/STOP are disabled, because a negative core id would map to an
 * invalid boot type. Pure logic, host-tested.
 */
#ifndef CORE_SELECT_H
#define CORE_SELECT_H

#define CORE_SELECT_NONE (-1)
#define CORE_SELECT_A55 0
#define CORE_SELECT_M33 1

/* New selection after `clicked` was clicked while `current` was selected. */
int core_select_toggle(int current, int clicked);
/* 1 if START/STOP may act on this selection. */
int core_select_can_operate(int selection);

#endif
