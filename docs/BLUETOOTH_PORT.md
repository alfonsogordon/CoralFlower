# Bluetooth MIDI Central integration investigation

Status: source structure inspected; no Bluetooth firmware implementation or hardware validation yet.

## Confirmed in CoralFlower
- Device compilation unit: `firmware/src/coralflower.c`, with header-only HAL and included C modules.
- Existing MIDI entry points: `usb.c` and (when enabled) `midi_uart.c`.
- UI: `cf_ui.c`, `cf_screens.c`; persistent settings: `cf_settings.c`.
- Device build: `./build.sh` -> `tools/build.py`; firmware image and `.fwsc` output.
- CI release workflow: `.github/workflows/release.yml` uses JieLi AC79 toolchain.
- Current build does not expose fm1-base's `fm1.toml` options.

## Confirmed in fm1-base README
- `ble = true` and `ble_central = true` enable BLE MIDI Central in that platform.
- `ble_stack = "own"` and `ble_pairing = true` select its own stack and bonding.
- APIs: `plat_ble_scan`, `plat_ble_found`, `plat_ble_connect`, `plat_ble_disconnect`, `plat_ble_device`, `plat_midi_in_from`.
- These APIs are **not** available by merely adding fm1.toml to CoralFlower: fm1-base uses a different platform abstraction.

## Integration tasks before a safe flashable build
1. Identify fm1-base radio implementation, linker memory layout, IRQ ownership and task servicing dependencies.
2. Compare against CoralFlower's `firmware/hal` and `firmware/src/main.c`.
3. Introduce BLE receive adapter at existing MIDI input dispatch (do not bypass sequencer clock and note logic).
4. Keep BLE off by default until a complete link and memory budget check passes.
5. Add scanner, connection status and persisted device selection only after transport works.
6. Run host syntax checks, CI device build, image size checks, then hardware testing.

## Initial test
Use an Android app capable of advertising BLE MIDI as a *peripheral* (not merely connecting to peripherals). Verify scan, connect, note on/off, CC, disconnect and reconnect.

Do not flash an unbuilt or unverified firmware. Preserve upstream CoralFlower and main branch.
