# Display bring-up debug — state as of 2026-08-11

## Resolution (2026-08-11)

**Fixed by reseating the DSI FFC cable/connector on the Mallow carrier.**
Confirms the suspected root cause below: a marginal physical connection
that ACKed I2C at idle but dropped out once the DSI PHY started clocking
and current draw/signal integrity on the link changed. No device tree,
kernel, or overlay change was needed — the overlay and configuration
described below were already correct.

## Goal

Enable the 10.1" DSI capacitive touch panel (Toradex PN **RVT101HVDWC00-B**,
1280x800 — user-typed "RVT101HVDNWC00-B" is a typo of this PN, confirmed
against Toradex's own overlay source) on Verdin Lino i.MX93 + Mallow via the
correct Torizon OS device tree overlay.

## What has been confirmed as fact (with commands)

1. **The correct overlay is `lino-imx93_panel-cap-touch-10inch-dsi_overlay.dts`.**
   Source: `git.toradex.com/cgit/device-tree-overlays.git`, branch
   `toradex_6.6-2.2.x-imx` (matches this target's kernel `6.6.142-7.7.0`).
   Confirmed by cloning the branch and grepping `overlays/` — this is the
   only overlay in the repo matching Lino i.MX93 + 10" DSI. A sibling file
   `display-rvt101hvdwc00b_overlay.dtsi` (included by the overlay) declares
   the panel node itself: `panel-lvds`, `vesa-24` mapping, 1280x800,
   66.3–78.9 MHz pixel clock range. This is a DSI-in / LVDS-out panel driven
   through a TI SN65DSI83 bridge — not a native DSI panel.

2. **The overlay is applied and active on target.**
   `cat /proc/device-tree/chosen/overlays*/name` → `overlays`.
   `dmesg` shows `/panel-rvt101hvdwc00b` and
   `/soc@0/bus@42000000/i2c@42530000/bridge@2c` nodes present in the live
   device tree, and `imx-drm display-subsystem: bound 4ae10000.dsi (ops
   dw_mipi_dsi_imx_ops)` — the DSI host bound successfully.

3. **The SN65DSI83 bridge driver is bound and its enable-gpio is asserted.**
   `sudo cat /sys/kernel/debug/gpio` shows `gpio-591 (LINO_P48 | enable) out
   hi` — this is the bridge's `enable-gpios` (Lino GPIO_10 / gpio4 pin 15
   per the overlay). `i2cdetect -y 2` on the correct bus (`lpi2c3` =
   `42530000.i2c` = `/dev/i2c-2`, confirmed via
   `/sys/bus/i2c/devices/i2c-2/of_node` → `.../i2c@42530000`) shows `UU` at
   address `0x2c` — the kernel driver owns that address; the chip is not
   silently absent from the bus.

4. **PLL lock fails with `-EIO` (errno 5), not `-ETIMEDOUT` (errno 110).**
   `dmesg`: `sn65dsi83 2-002c: failed to lock PLL, ret=-5`. Read directly
   from `drivers/gpu/drm/bridge/ti-sn65dsi83.c` (torvalds/linux, current
   master): the driver polls `REG_RC_LVDS_PLL` via
   `regmap_read_poll_timeout()`, which returns `-ETIMEDOUT` on a genuine
   PLL-never-locked timeout, or propagates whatever error the underlying
   `regmap_read()` (an I2C transaction) returned. `-EIO` specifically means
   **the I2C read of the status register failed** — the chip stopped
   responding on I2C at the moment the driver polled it, after having
   ACKed everything up through PLL_EN being written.

5. **No `vcc-supply` is wired for the bridge in the Toradex overlay.**
   `dmesg`: `sn65dsi83 2-002c: supply vcc not found, using dummy regulator`.
   This is the overlay as shipped by Toradex, not a local mistake — the
   dummy regulator means Linux has no way to sequence or verify the
   bridge's actual power rail.

## Actively suspected root cause

The bridge ACKs I2C during driver probe and register bring-up (address
`0x2c` responds, `enable-gpios` is high, PLL_EN gets written), then goes
unresponsive on I2C specifically at the moment DSI PHY clocks start
toggling and the driver polls for lock. This pattern — chip fine at idle,
chip drops off I2C once the DSI link starts clocking — points at a
power-integrity issue on the bridge/panel rail (brownout or noise induced
by the DSI PHY starting up) rather than a device-tree misconfiguration.

Confidence: **hypothesis, not fact**. Cannot confirm without either a scope
on the bridge's VCC rail across the exact moment `PLL_EN` is asserted, or
swapping in a known-good display/cable to rule out a marginal FFC/board
connection.

## What has been ruled out

- **Wrong overlay / wrong panel selection.** Ruled out — overlay matches
  Toradex's own source exactly for this hardware combo (Lino + Verdin
  adapter + Mallow + RVT101HVDWC00-B), and it is visibly active in the live
  device tree.
- **Wrong I2C bus number.** Ruled out — `lpi2c3`/`42530000.i2c` maps to
  `/dev/i2c-2`, confirmed via sysfs `of_node`, and that is the bus probed.
- **enable-gpios stuck low / bridge never taken out of reset.** Ruled out —
  `gpio-591` reads `out hi`.
- **Bridge fully absent from the bus (dead chip, no power at all, or wrong
  I2C address).** Ruled out — `i2cdetect` shows `UU` (driver-bound) at
  `0x2c`, not `--`.
- **PLL genuinely never locking due to a lane-count/clock-rate mismatch**
  (would surface as `-ETIMEDOUT`/-110, not `-EIO`/-5). Downgraded in
  priority given the specific errno observed, but not fully eliminated —
  a clock/lane mismatch could still be an indirect contributor if it
  causes the current draw spike that then brings down I2C.

## Next moves (not yet attempted)

1. Scope or multimeter the SN65DSI83 VCC/VCORE rail during the exact boot
   window (~16.3–16.5 s per dmesg timestamps) to catch a brownout when DSI
   PHY clocks start.
2. Re-run `i2cdetect -y 2` immediately after a failed boot (without
   rebooting) to see whether `0x2c` still shows `UU`/ACKs at all post-
   failure, or has gone fully silent — distinguishes "transient glitch
   during clock start" from "chip permanently wedged until power-cycle."
3. Physically reseat/inspect the DSI FFC cable and connector on the Mallow
   carrier — a marginal connection is consistent with a fault that only
   appears once the link is driven at speed.
4. If reproducible, escalate to Toradex support / community forum with
   this evidence chain — the `-EIO`-on-PLL-poll signature has at least one
   precedent on a different Toradex SoM+bridge combo
   (community.toradex.com thread on Verdin iMX8MM + Mallow + Riverdi
   RVT70HSDFWCA0 hit a related, but not identical, SN65DSI83 bring-up
   fault).
