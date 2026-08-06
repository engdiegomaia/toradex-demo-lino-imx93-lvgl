/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms.
*/

#ifndef __HOME_STATUS_H__
#define __HOME_STATUS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Live per-core state as observed by the backend, decoupled from any of its
 * internal structs so a future GUI-Guider regeneration only breaks the
 * include line in gg_Home.c (2 lines to reapply), not the polling logic. */
typedef enum {
    HOME_STATE_IDLE = 0,      /* firmware not started */
    HOME_STATE_BOOTING,       /* start.sh launched, handshake pending */
    HOME_STATE_RUNNING,       /* handshake reached and shm ring is live */
    HOME_STATE_FAILED,        /* handshake timed out or start.sh failed */
} home_core_state_t;

typedef struct {
    home_core_state_t state;
    int link_live;            /* 1 when shared-memory ring produced data recently */
} home_core_status_t;

/* Poll and repaint Home widgets from backend state. Called from the LVGL
 * thread only; no locking required for widget access.
 *
 * Reads OperationInfo (mutex-protected) + core_session_t (single-writer
 * from backend main loop, safe unlocked int reads on arm64). */
void home_status_tick(void);

#ifdef __cplusplus
}
#endif
#endif /* __HOME_STATUS_H__ */
