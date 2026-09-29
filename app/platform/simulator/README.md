# LVGL Simulator

This directory contains the desktop simulator configuration for the GUI
application. The simulator is optional; the primary deployment target is the
Toradex Verdin i.MX93 running Torizon OS.

## Build

Use a host toolchain supported by the CMake project and provide SDL2. From this
directory, configure and build in an out-of-tree directory:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

SDL2 discovery depends on the host platform. Install SDL2 development files or
set the CMake package path as needed. Do not commit the generated `build/`
directory.

The simulator does not reproduce the target's remoteproc, shared-memory,
Wayland, or hardware display behavior.
