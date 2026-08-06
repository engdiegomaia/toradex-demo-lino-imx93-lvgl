## Industrial HMI with Real-time Edge

Industrial HMI projects often involve more demanding requirements for responsiveness, maintainability, and integration with real system data. In such projects, GUI Guider is most effective when used as one part of a structured system architecture.

Industrial HMI with Real-time Edge leverages NXP's Real-time Edge software to deliver responsive, low-latency human-machine interfaces for industrial environments. By using Preempt-RT Linux, RTOS, and BareMetal frameworks on heterogeneous multicore and multi-SoC platforms, it ensures fast data processing and real-time control.

This solution supports a rich set of industrial networking protocols, including EtherCAT, CANopen, and Modbus, enabling integration with sensors, encoders, and field devices. Combined with optimized graphics and unified life cycle management across cores, it provides a robust foundation for reliable, high-performance industrial HMI systems.

For more information, visit the [Real-Time Edge website](https://www.nxp.com/design/design-center/software/real-time-edge-software:REALTIME-EDGE-SOFTWARE).

### Industrial HMI application setup

This section provides step-by-step instructions for setting up an industrial HMI application based on NXP Real-time Edge V3.4.

**Prerequisites**

- NXP Real-time Edge V3.4 image
- MCIMX93FRDM
- SD card (8 GB minimum recommended)
- SD card reader/writer
- Linux host computer for preparation

**Setup instructions**

1. Deploy the pre-built SD card image:

   1. Download the Real-time Edge V3.4 image (`REAL-TIME_EDGE_V3.4_IMX93-11XI-LPDDR4-FRDM`) from the [NXP download page](https://www.nxp.com/webapp/Download?colCode=REAL-TIME_EDGE_V3.4_IMX93-11XI-LPDDR4-FRDM&appType=license).
   2. Flash the image to the SD card, for example using Balena Etcher or `dd`:

      ```
      unzip Real-time_Edge_v3.4_IMX93-11X11-LPDDR4X-FRDM.zip
      cd Real-time_Edge_v3.4_IMX93-11X11-LPDDR4X-FRDM/real-time-edge
      zstd -d -f nxp-image-real-time-edge-imx93-11x11-lpddr4x-frdm.rootfs.wic.zst
      sudo dd if=nxp-image-real-time-edge-imx93-11x11-lpddr4x-frdm.rootfs.wic of=/dev/sdX bs=4M && sync
      ```

2. Copy the DTB file. Insert the SD card into the target board and power it on. Copy the required DTB file from the `custom/dtb` directory to the boot partition:

   ```
   sudo cp custom/imx93-11x11-frdm-multicore-rtos-waveshare-7inch-c-panel.dtb /run/media/$USER/boot-mmcblk1p1/
   ```

3. Configure the U-Boot environment:

   1. Insert the SD card into the target board and power it on.
   2. Interrupt the boot process to enter the U-Boot console.
   3. Set the DTB and save the environment:

      ```
      setenv fdtfile imx93-11x11-frdm-multicore-rtos-waveshare-7inch-c-panel.dtb
      setenv mmcargs $mmcargs clk_ignore_unused
      saveenv
      ```

4. Upload the application files to the target board:

   ```
   scp -r custom/elf/ root@<board-ip>:/path/to/destination/
   scp -r custom/script/* root@<board-ip>:/path/to/destination/
   ```

5. Click **Generate Code** in GUI Guider.

6. Click **Build Target** in GUI Guider and upload the `gui_guider` binary:

   ```
   scp -r platform/Linux/build/gui_guider root@<board-ip>:/path/to/destination/
   ```

7. Run the application:

   > **Note:** Before running the application, restart the Weston service to avoid issues related to service state or configuration.

   ```
   systemctl restart weston
   ./gui_guider
   ```

   The expected file structure is:

   ```
   elf/ gui_guider start.sh stop.sh
   ```

**Troubleshooting**

- If the display does not work, verify that the correct DTB file is in use.
- For input device issues, check the input device paths in your configuration.
- Inspect system logs for error messages:

   ```
   dmesg | tail
   ```

### Yocto build with Real-time Edge

This section provides step-by-step instructions for building a Yocto environment with GUI Guider support for the MCIMX93FRDM using the Real-time Edge V3.4 distribution.

**Prerequisites**

- Git and the `repo` tool installed.
- Sufficient disk space (50 GB minimum recommended).
- A properly configured Linux build environment.
- MCIMX93FRDM
- NXP Real-time Edge V3.4 image.

1. Download Real-time Edge V3.4 and initialize the workspace:

   ```
   mkdir yocto-real-time-edge-v3.4
   cd yocto-real-time-edge-v3.4
   repo init -u https://github.com/nxp-real-time-edge-sw/yocto-real-time-edge.git -b real-time-edge-walnascar -m real-time-edge-3.4.0.xml
   repo sync
   ```

2. Set up the build environment for the MCIMX93FRDM:

   ```
   DISTRO=nxp-real-time-edge MACHINE=imx93-11x11-lpddr4x-frdm source real-time-edge-setup-env.sh -b build-imx93-frdm
   ```

   > **Note:** For detailed setup instructions, see the [Real-Time Edge v3.4 README](https://github.com/nxp-real-time-edge-sw/yocto-real-time-edge/blob/Real-Time-Edge-v3.4-202604/README.md).

3. Add the GUI Guider meta layer:

   1. Copy the exported `meta-gui-guider` folder to the sources directory:

      ```
      cp -r /path/to/meta-gui-guider ./sources/
      ```

   2. Add the layer to the build environment:

      ```
      bitbake-layers add-layer ../sources/meta-gui-guider
      ```

4. Configure the build and build GUI Guider:

   ```
   echo "INHERIT += \"rm_work\"" >> conf/local.conf
   echo "RM_WORK_EXCLUDE += \"gui-guider\"" >> conf/local.conf
   bitbake gui-guider
   ```

   Build configuration explanation:

   - `INHERIT += "rm_work"`: Enables automatic cleanup of temporary work directories to save disk space.
   - `RM_WORK_EXCLUDE += "gui-guider"`: Keeps the `gui-guider` package out of automatic cleanup, preserving build artifacts for debugging.

5. After a successful build:

   1. Upload the generated binary to the MCIMX93FRDM.
   2. Verify GUI Guider application functionality on the board.
   3. Refer to the application documentation for usage details.
