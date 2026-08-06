/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __CUSTOM_H_
#define __CUSTOM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"


typedef struct {
    int16_t coreId; 
    int16_t appName;
    int16_t status;
    int16_t trigger;
} OperationInfo;

void custom_init(gg_ui_t *ui);
void radio_event_handler(lv_event_t * e);
void stop_refresh_anim(int cortexID);
void request_stop_refresh_anim(int cortexID);
extern OperationInfo * realTime_operation;

/*
* Initializes the operation (running or stop) application info.
*/
void init_operation_info();
/*
* Update the operation (running or stop) application info. 
* Parameters:
*   coreid: unique identifier for the coretex.
*   appname: the application name id.
*   status: mean that running or stop application. 0 on stop, 1 on start. 
*   trigger: mean that the application whether already triggered.
* Returns:
*   0 on success, -1 on failure
*/
int update_operation_info(int16_t coreid, int16_t appname, int16_t status, int16_t isTrigger);
/*
* get the operation info. operationInfo->trigger: true on the application is need to be triggered, false on the application already triggered.
*/
OperationInfo* get_operation_info();
int copy_operation_info(OperationInfo *info);

/*
* update the operation (running or stop) application trigger status.
*/
int update_operation_status(int16_t trigger);

/*
* modify the code base from lvgl repo chart lv_example_chart_3.c
*/
void chart_event_cb(lv_event_t * e);


#ifdef __cplusplus
}
#endif
#endif /* EVENT_CB_H_ */
