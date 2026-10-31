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

## MIDI queue concurrency audit

Verified `firmware/src/cf_out.c` `cf_midi_in()`: audio processing snapshots `mi_w`, drains `midi_in_q` and uses `midi_in_ms` for timing. Existing USB receive runs from TIMER5 ISR and UART shares the same ring. `midi_in_overflow` causes an early return until the existing overflow handler releases notes. Bluetooth must not introduce an unsynchronized additional producer.

Before integrating the radio:
- Use BLE's own interrupt-to-main-loop ring, then hand off to a carefully serialized producer path. Do not enqueue from BLE radio IRQ.
- Review whether main-loop enqueue is safe against TIMER5 USB/UART enqueue; protect queue publication with a minimal critical section or redesign as single producer.
- Reserve USB receive headroom (EP1_ROOM 24 slots) and prevent BLE traffic from starving USB or TRS.
- Keep timestamps consistent with `fm1_ms`; ensure all-notes-off on disconnect to prevent stuck notes.
- Do not expose BLE clock until real-time packets are handled with the same semantics as USB.

**Recovery accessory:** the user plans to purchase an RP-series board. Do not assume a generic RP2040/RP2350 board alone can restore the FM-1. Confirm exact recovery procedure, voltage, pinout, software image and wiring from reliable device-specific instructions before suggesting connections; no flash/erase without verified backup.

## Implemented: staged BLE IRQ-to-TIMER5 bridge

Commits on `feature/bluetooth-midi` add a 32-event SPSC ingress ring in `cf_ble_midi.h` and call `cf_ble_midi_drain()` from TIMER5 after USB and TRS polling. Per service call it forwards at most four events and stops when 24 shared MIDI slots remain, preserving the existing USB EP1 reservation. Invalid or unsupported packets are rejected. The ring overflows by dropping new BLE events and recording a counter, not by setting the existing USB/TRS global MIDI overflow flag.

`tests/cf_ble_midi_test.c` covers valid/invalid packets, bounded drain, reserved headroom and ingress overflow; the GitHub `check` workflow is configured to run it.

**Important remaining correctness work before enabling the radio:** BLE ingress currently assumes exactly one IRQ producer; validate the donor callback's execution context and interrupt nesting. BLE message drops can leave notes stuck: add disconnect and overflow recovery, preferably tracking active notes and issuing releases through the established audio MIDI path. Measure IRQ timing and ensure TIMER5 never starves the audio renderer. No verified device build or hardware testing yet.

## Staged disconnect/overflow recovery

The BLE ingress ring now increments `cf_ble_faults` on dropped events or an explicit `cf_ble_midi_disconnect()`. TIMER5 notices a new fault and discards pending BLE events; `cf_out.c` audio `events_block()` notices the monotonic fault counter and requests the existing all-part voice-release path. This avoids direct voice manipulation from radio IRQ. Tests cover overflow/disconnect ring flush.

**Trade-off:** this releases all parts, including USB/TRS-held notes, on a BLE fault. It is deliberately conservative until per-source note ownership exists; refine before shipping. The BLE driver must call `cf_ble_midi_disconnect()` on every actual link loss. The current code has no radio driver yet, so the callback is not connected.

**Critical:** GitHub workflow run success and device link have not been confirmed. Code commits are not proof of a tested firmware image.

## Compile-time isolation (latest)

`CF_BLE_MIDI` defaults to **0** in `coralflower.c`. The staged adapter include, TIMER5 drain and audio-block BLE fault release are all conditional on `CF_BLE_MIDI`. Existing firmware builds therefore do not execute or allocate the experimental BLE ingress ring. The `check` GitHub workflow compiles both default and `-DCF_BLE_MIDI=1` variants and runs the BLE host regression test.

Enabling `CF_BLE_MIDI=1` enables **only the ingress bridge**, not the BLE radio, central scanning, GATT or pairing. Never label such a binary BLE-ready. CI run results have not yet been independently confirmed.

## BLE Central implementation boundary

The donor `charlesvestal/fm1-base/device/bt_own_central.c` exposes `plat_ble_scan`, `plat_ble_found`, `plat_ble_connect`, `plat_ble_disconnect`, and `plat_ble_device`. These are not standalone: they require the donor's `bt/ble/ble_stack.c` aggregation, `device/bt_own.c`, `device/radio.c`, hardware BLE HAL `fm1_ble.h`, calibration trim sources, and their linker/IRQ setup. Copying only `bt_own_central.c` would not yield a functional radio.

For CoralFlower, wire the donor callback `ble_app_midi_in(packet, ts, last_ts)` to `cf_ble_midi_receive(packet)` **only after** checking the callback's interrupt context, packet packing, memory barriers and single-producer assumption. Wire link-loss notification to `cf_ble_midi_disconnect()`. BLE-MIDI timestamps are currently discarded in favour of the existing shared queue's `fm1_ms`; evaluate jitter before release.

Central UI must offer scan, discovered names, connect, disconnect and status. SMC-Mixer discovery must use BLE MIDI service/GATT rather than trusting device name alone. Confirm licensing on every imported donor file. Validate memory map, RF calibration and bootloader boundaries before any device image is produced.

The radio stack has **not** been imported or initialized. The latest transport bridge also rechecks faults between forwarded messages to stop a mid-drain disconnect from releasing more stale events.

## Link-generation tagging (staged)

The ingress ring now carries a `cf_ble_epoch` alongside each MIDI packet. A link-loss notification or ingress overflow increments the epoch; TIMER5 discards entries whose generation no longer matches. This complements the fault counter and audio-side voice release, and is intended to prevent queued MIDI events from a previous connection being forwarded after reconnect.

The actual BLE driver **must** call `cf_ble_midi_disconnect()` when a connection ends. Without the radio integration this is not yet exercised against real link events. The ring remains single-producer/single-consumer; multiple BLE IRQ callback contexts would invalidate this assumption and must be audited. A successful compile, RF initialization, BLE-MIDI discovery and hardware test are still pending.
