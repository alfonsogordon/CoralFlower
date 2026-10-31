/* SPDX-License-Identifier: GPL-3.0-only
 * CoralFlower Bluetooth MIDI receive adapter.
 *
 * STAGING ONLY: no radio is initialized or linked by this file yet.
 * Include after usb.c in the CoralFlower unity build, then call
 * cf_ble_midi_receive() from the eventual BLE main-loop ring drain.
 *
 * BLE radio interrupts must NOT call midi_enqueue directly: the existing
 * USB/TRS MIDI ring is consumed by the audio interrupt and its producer
 * assumptions must be checked before introducing another IRQ producer.
 */
#ifndef CF_BLE_MIDI_ADAPTER_H
#define CF_BLE_MIDI_ADAPTER_H

/* The BLE stack supplies a USB-MIDI event packet: cable/CIN in byte 0,
 * status in byte 1, data bytes in bytes 2-3. Discard the donor's
 * cable number (which identifies the BLE role, not a CoralFlower part).
 * The queue's existing timestamp uses fm1_ms. */
static int __attribute__((unused)) cf_ble_midi_receive(uint32_t packet)
{
    uint32_t cin = packet & 0x0fu;
    uint32_t status = (packet >> 8) & 0xffu;
    uint32_t data1 = (packet >> 16) & 0xffu;
    uint32_t data2 = (packet >> 24) & 0xffu;
    /* Stage 1: accept channel voice only. SysEx, real-time clock and
     * system messages require explicit parser/source review before enabling. */
    if (cin < 8u || cin > 0xeu || status < 0x80u || status >= 0xf0u)
        return 0;
    if ((status >> 4) != cin || (data1 & 0x80u))
        return 0;
    if (cin != 0xcu && cin != 0xdu && (data2 & 0x80u))
        return 0;
    return midi_enqueue((packet & ~0xf0u), 3u);
}
#endif
