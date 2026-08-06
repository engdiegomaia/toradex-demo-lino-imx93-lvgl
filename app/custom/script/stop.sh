#!/bin/bash
# Copyright 2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
# activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
# terms, then you may not retain, install, activate or otherwise use the software.
#

CORE=$1

ELF_DIR=/examples/heterogeneous-multicore
ACMD_DIR="/sys/devices/platform/remoteproc-ca55-1/remoteproc/remoteproc0"
MCMD_DIR="/sys/devices/platform/remoteproc-cm33/remoteproc/remoteproc1"

if [ "$CORE" == "a" ]; then
    echo stop > $ACMD_DIR/state
else
    echo stop > $MCMD_DIR/state
fi
