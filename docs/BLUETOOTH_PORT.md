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

## Flash safety gate — mandatory before user testing

**Current status: NO FLASH.** No BLE driver has been integrated or built.

Confirmed by inspecting CoralFlower `firmware/src/main.c`, `firmware/src/usb.c`, `firmware/src/coralflower.c` and `tools/build.py`:

- The boot stub validates a loader commit marker and app CRC before entering the app. OCT- at power-on is documented as a recovery key; OCT- + OCT+ triggers app calibration instead. Verify actual behavior on hardware and with the exact installer before relying on it.
- Boot guard records early resets and enters SAFE MODE after repeated failures, eventually UBOOT. This only helps if the boot stub and recovery path remain intact.
- The app's USB MIDI queue has 64 entries and USB receives in TIMER5 IRQ; audio ISR drains it. BLE IRQ must not write that queue directly. A main-loop bridge still needs concurrency analysis because USB/TIMER5 can preempt it.
- The USB MIDI parser handles SysEx, clock and status validation separately. The staging BLE adapter therefore accepts **channel voice only** and cannot yet provide clock or SysEx.
- The BLE adapter header is **not included** by `coralflower.c`; it has no effect on any firmware image yet.

### Required preflash evidence

1. Verify original fork commit and compare proposed changes against main.
2. Build unmodified main with the same toolchain as the BLE branch; capture output hashes and memory section sizes.
3. Confirm linker addresses and reserved flash ranges for boot stub, update loader, settings, calibration and BLE storage; no overlapping radio persistence writes.
4. Compile BLE branch with strict warnings and validate full link; no unresolved references.
5. Ensure radio startup is disabled in SAFE MODE and failure falls back to existing non-BLE boot.
6. Verify radio IRQ priorities, critical sections, watchdog feeding and measured audio worst-case load.
7. Confirm firmware identity and installer accept only the intended FM1 target; no automatic deployment.
8. Verify recovery procedure and availability of known-good firmware **before** any user flash.
9. Only after those gates, prepare a clearly labeled experimental package and ask for explicit permission to test.

No claims of brick-proof operation; flashing always carries residual risk.
