/* SPDX-License-Identifier: GPL-3.0-only
 * BLE MIDI ingress bridge, staging only. No BLE radio is enabled here.
 *
 * Exactly one BLE IRQ producer calls cf_ble_midi_receive(); TIMER5 is the
 * only consumer and forwards to the existing USB/TRS MIDI ring. Never call
 * midi_enqueue from a BLE IRQ or from the main loop.
 */
#ifndef CF_BLE_MIDI_ADAPTER_H
#define CF_BLE_MIDI_ADAPTER_H
#define CF_BLE_IQ 32u
#define CF_BLE_TICK_BUDGET 4u
#define CF_BLE_USB_RESERVE 24u
static uint32_t cf_ble_iq[CF_BLE_IQ];
static volatile uint32_t cf_ble_iw, cf_ble_ir, cf_ble_dropped, cf_ble_faults;

static int __attribute__((unused)) cf_ble_midi_receive(uint32_t packet)
{
    uint32_t cin = packet & 0x0fu;
    uint32_t status = (packet >> 8) & 0xffu;
    uint32_t data1 = (packet >> 16) & 0xffu;
    uint32_t data2 = (packet >> 24) & 0xffu;
    uint32_t w = cf_ble_iw;
    /* Only valid channel voice. System, SysEx and clock are not ready. */
    if (cin < 8u || cin > 0xeu || status < 0x80u || status >= 0xf0u ||
        (status >> 4) != cin || (data1 & 0x80u) ||
        (cin != 0xcu && cin != 0xdu && (data2 & 0x80u))
        return 0;
    if (w - cf_ble_ir >= CF_BLE_IQ) {
        cf_ble_dropped++;
        cf_ble_faults++; /* Lost note-off may stick: audio must release notes. */
        return 0;
    }
    cf_ble_iq[w % CF_BLE_IQ] = packet & ~0xf0u;
    RING_PUBLISH();
    cf_ble_iw = w + 1u;
    return 1;
}

/* Called on link loss from the BLE producer context, not the audio ISR. */
static void __attribute__((unused)) cf_ble_midi_disconnect(void)
{
    cf_ble_faults++;
}

/* Call only from TIMER5's non-nested MIDI service, after USB and UART.
 * Headroom ensures a BLE burst cannot consume the USB EP1 reservation.
 * The BLE producer must be a single IRQ context; IRQ priorities and
 * disconnect note-release behavior still require hardware validation.
 */
static void __attribute__((unused)) cf_ble_midi_drain(void)
{
    static uint32_t seen_faults;
    uint32_t n = 0;
    if (seen_faults != cf_ble_faults) {
        seen_faults = cf_ble_faults;
        cf_ble_ir = cf_ble_iw; /* Discard stale note-ons after a fault. */
        RING_PUBLISH();
        return;
    }
    while (cf_ble_ir != cf_ble_iw && n < CF_BLE_TICK_BUDGET &&
           !midi_in_overflow && MQ - (mi_w - mi_r) > CF_BLE_USB_RESERVE) {
        /* A radio fault can arrive while TIMER5 drains. Never forward
         * messages observed after that fault, even in this same tick. */
        if (seen_faults != cf_ble_faults) {
            seen_faults = cf_ble_faults;
            cf_ble_ir = cf_ble_iw;
            RING_PUBLISH();
            return;
        }
        uint32_t packet = cf_ble_iq[cf_ble_ir % CF_BLE_IQ];
        if (!midi_enqueue(packet, 3u))
            break;
        RING_PUBLISH();
        cf_ble_ir++;
        n++;
    }
}
#endif
