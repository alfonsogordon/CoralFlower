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


## Source-level donor mapping (31 Oct 2026)

GitHub code search and direct file retrieval verified these donor paths:
- `device/bt_own_central.c`: FM1-facing scan, connect, disconnect, state and passkey APIs. It is included as part of `device/bt_own.c`; **not** a standalone module.
- `bt/ble/ble_central.c`: BLE initiator connection state machine, encryption/pairing and GATT client subscription.
- `bt/ble/ble_gattc.c`: GATT client used to discover and subscribe to BLE-MIDI characteristics.
- `bt/ble/ble_host.c`, `bt/ble/ble_stack.c`, `bt/ble/ble_smp_init.c`: supporting BLE host, stack and pairing.
- `device/radio.c`: radio startup, timing, interrupt masking, watchdog, callbacks and scheduler integration.
- `lib/btdev.c`, `lib/btdev.h`: higher-level device management.
- `docs/bt/BLE-DEVICES-DESIGN.md`: intended discovery, saved-device and reconnect behavior.

Important integration findings:
- Donor `device/bt_own_central.c` depends on symbols such as `own_held`, `ble_state`, `shim_ms`, `ble_ll_scan`, `ble_scan_add`, `ble_central_connect` and `ota_wire_send`; copying it alone will not link.
- Donor `device/radio.c` provides a nested IRQ guard and millisecond timing based on FM1 ticks. These must be reconciled with CoralFlower's existing HAL and audio timing before enabling the radio.
- CoralFlower uses `firmware/src/coralflower.c` as its unity build, and currently includes `usb.c` and `midi_uart.c`. The adapter should enter the existing MIDI receive dispatch, not synth note handling directly.
- Donor documentation says one active BLE link at a time. Device role changes and Android peripheral pairing require validation.
- Donor `bt_own_central.c` explicitly notes that central peer bonds are not yet persisted in its key callback. Do **not** promise automatic bonded reconnection without implementing and testing persistence.

Next step: inspect full donor `device/bt_own.c`, `bt/ble/ble_stack.c` and CoralFlower `firmware/src/main.c` and `firmware/src/usb.c`; then define a build-flagged BLE transport adapter. Do not introduce untested radio init into the production boot path.
