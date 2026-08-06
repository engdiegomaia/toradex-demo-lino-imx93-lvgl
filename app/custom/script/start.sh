#!/bin/bash
# Copyright 2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
# accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
# activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
# terms, then you may not retain, install, activate or otherwise use the software.
#

CORE=$1
RTOS=$2
TYPE=$3

ACMD_DIR="/sys/devices/platform/remoteproc-ca55-1/remoteproc/remoteproc0"
MCMD_DIR="/sys/devices/platform/remoteproc-cm33/remoteproc/remoteproc1"
ELF_DIR=$PWD/elf

#./stop.sh $CORE

if [ "$CORE" == "a" ]; then
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then
            echo $ELF_DIR/hello_world_ca55_0xd0100000_zephyr.elf > $ACMD_DIR/firmware
        else
            echo $ELF_DIR/rt_latency_ca55_0xd0100000_zephyr.elf > $ACMD_DIR/firmware
        fi
    else
        if [ "$TYPE" == "h" ]; then
            echo $ELF_DIR/hello_world_ca55_0xd0fff000.elf > $ACMD_DIR/firmware
        else
            echo $ELF_DIR/rt_latency_ca55_0xd0fff000.elf > $ACMD_DIR/firmware
        fi
    fi
    echo start > $ACMD_DIR/state
else
    if [ "$RTOS" == "z" ]; then
        if [ "$TYPE" == "h" ]; then
            echo -n $ELF_DIR/hello_world_cm33_zephyr.elf > $MCMD_DIR/firmware
        else
            echo -n $ELF_DIR/rt_latency_cm33_zephyr.elf > $MCMD_DIR/firmware
        fi
    else
        if [ "$TYPE" == "h" ]; then
            echo -n $ELF_DIR/hello_world_cm33.elf > $MCMD_DIR/firmware
        else
            echo -n $ELF_DIR/rt_latency_cm33.elf > $MCMD_DIR/firmware
        fi
    fi
    echo start > $MCMD_DIR/state
fi
