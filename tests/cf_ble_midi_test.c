/* SPDX-License-Identifier: GPL-3.0-only
 * Host regression of BLE ingress ring and TIMER5 forwarding.
 */
#include <stdint.h>
#include <assert.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define MQ 64u
static uint32_t mi_w, mi_r;
static uint8_t midi_in_overflow;
static uint32_t seen, seen_source, count;
static int midi_enqueue(uint32_t packet, uint32_t source)
{
    if (mi_w - mi_r >= MQ) return 0;
    seen = packet; seen_source = source; count++; mi_w++;
    return 1;
}
#include "../firmware/src/cf_ble_midi.h"
static uint32_t pkt(uint8_t cin, uint8_t st, uint8_t d1, uint8_t d2)
{
    return (uint32_t)(0x10u | cin) | (uint32_t)st << 8 |
           (uint32_t)d1 << 16 | (uint32_t)d2 << 24;
}
int main(void)
{
    uint32_t i;
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,100))); /* Not connected. */
    cf_ble_midi_link_changed(1);
    assert(cf_ble_link_up == 1 && cf_ble_epoch == 1);
    cf_ble_midi_link_changed(1);
    assert(cf_ble_epoch == 1); /* Duplicate status is harmless. */
    assert(cf_ble_midi_receive(pkt(9,0x90,60,100)));
    assert(cf_ble_midi_receive(pkt(8,0x80,60,0)));
    assert(cf_ble_midi_receive(pkt(0xa,0xa0,60,70)));
    assert(cf_ble_midi_receive(pkt(0xb,0xb0,74,127)));
    assert(cf_ble_midi_receive(pkt(0xc,0xc0,12,0)));
    assert(cf_ble_midi_receive(pkt(0xd,0xd0,75,0)));
    assert(cf_ble_midi_receive(pkt(0xe,0xe0,0,64)));
    assert(!cf_ble_midi_receive(pkt(0,0x90,60,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x80,60,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x90,0x80,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,0x80)));
    assert(!cf_ble_midi_receive(pkt(0xf,0xf8,0,0)));
    assert(!cf_ble_midi_receive(pkt(4,0xf0,0,0)));
    assert(count == 0);
    cf_ble_midi_drain();
    assert(count == 4 && seen_source == 3);
    cf_ble_midi_drain();
    assert(count == 7 && seen == (pkt(0xe,0xe0,0,64) & ~0xf0u));
    assert(cf_ble_ir == cf_ble_iw);
    /* Never consume the USB EP1 headroom. */
    mi_w = MQ - CF_BLE_USB_RESERVE;
    assert(cf_ble_midi_receive(pkt(9,0x90,61,90)));
    cf_ble_midi_drain();
    assert(count == 7 && cf_ble_ir != cf_ble_iw);
    mi_r = mi_w;
    cf_ble_midi_drain();
    assert(count == 8 && cf_ble_ir == cf_ble_iw);
    /* Bounded ingress ring: overflow drops rather than corrupts. */
    for (i = 0; i < CF_BLE_IQ; i++)
        assert(cf_ble_midi_receive(pkt(9,0x90,60,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,100)));
    assert(cf_ble_dropped == 1);
    assert(cf_ble_faults == 1);
    cf_ble_midi_drain();
    assert(cf_ble_iw == cf_ble_ir); /* Stale notes flushed on loss. */
    assert(cf_ble_midi_receive(pkt(9,0x90,60,100)));
    cf_ble_midi_disconnect();
    assert(cf_ble_faults == 2);
    assert(cf_ble_epoch == 3);
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,100))); /* Disconnected. */
    cf_ble_midi_link_changed(1);
    cf_ble_midi_drain();
    assert(cf_ble_iw == cf_ble_ir); /* Disconnect flushes stale notes. */
    /* A fault is detected before forwarding any pending event. */
    assert(cf_ble_midi_receive(pkt(9,0x90,62,100)));
    cf_ble_midi_disconnect();
    cf_ble_midi_drain();
    assert(cf_ble_ir == cf_ble_iw);
    assert(cf_ble_epoch == 5);
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,100)));
    cf_ble_midi_link_changed(1);
    assert(cf_ble_epoch == 6);
    assert(cf_ble_midi_receive(pkt(9,0x90,60,100)));
    midi_in_overflow = 1;
    cf_ble_midi_drain();
    assert(cf_ble_iw - cf_ble_ir == 1);
    return 0;
}
