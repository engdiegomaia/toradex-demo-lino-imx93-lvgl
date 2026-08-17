/*
* Copyright 2025 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __GUI_GUIDER_H__
#define __GUI_GUIDER_H__

#include "basedef.h"

#define FRTOS_RAM_ADDR (0xD0FFF000)
#define ZEPHY_RAM_ADDR (0xD0100000)

/*
 * Base of the raw shared-memory struct (rt_latency_shm_t) polled by the
 * Linux side. This is the start of the M33's non-cached window — upstream
 * NXP calls it m_data2 and places .ncache/.ncache.init there.
 *
 * Three places must agree on this address:
 *   1. here (Linux /dev/mem mapping),
 *   2. cm33-shm@a5000000 in
 *      device-tree/overlays/lino-imx93_cm33-shm_overlay.dts,
 *   3. ORIGIN(m_data2) in the firmware's linker script
 *      (MIMX9352_cm33_ram.ld). The producer uses its
 *      __noncachedata_start__ symbol as the explicit shared pointer.
 *
 * Two addresses that look plausible but are WRONG on Verdin i.MX93:
 *   0xA4000000 — the original FRDM_SHM_BASE from the FRDM-IMX93 port. The
 *                base DT maps vdev0vring0 there, so this polled virtio
 *                vring memory owned by the RPMsg transport.
 *   0xA0000000 — upstream's EVK value for m_data2. /proc/iomem here shows
 *                "98000000-a3ffffff : System RAM", i.e. live kernel memory.
 *
 * See docs/M33_HANDSHAKE_DEBUG.md.
 */
#define FRDM_SHM_BASE (0xA5000000)

#endif
