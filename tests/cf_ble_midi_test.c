/* SPDX-License-Identifier: GPL-3.0-only
 * Host-only regression test of the staged BLE MIDI packet filter.
 * No FM-1 hardware or radio access.
 */
#include <stdint.h>
#include <assert.h>
static uint32_t seen, seen_source, count;
static int midi_enqueue(uint32_t packet, uint32_t source)
{
    seen = packet; seen_source = source; count++;
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
    assert(cf_ble_midi_receive(pkt(9,0x90,60,100)));
    assert(seen_source == 3 && seen == (pkt(9,0x90,60,100) & ~0xf0u));
    assert(cf_ble_midi_receive(pkt(8,0x80,60,0)));
    assert(cf_ble_midi_receive(pkt(0xa,0xa0,60,70)));
    assert(cf_ble_midi_receive(pkt(0xb,0xb0,74,127)));
    assert(cf_ble_midi_receive(pkt(0xc,0xc0,12,0)));
    assert(cf_ble_midi_receive(pkt(0xd,0xd0,75,0)));
    assert(cf_ble_midi_receive(pkt(0xe,0xe0,0,64)));
    assert(count == 7);
    assert(!cf_ble_midi_receive(pkt(0,0x90,60,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x80,60,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x90,0x80,100)));
    assert(!cf_ble_midi_receive(pkt(9,0x90,60,0x80)));
    assert(!cf_ble_midi_receive(pkt(0xf,0xf8,0,0)));
    assert(!cf_ble_midi_receive(pkt(4,0xf0,0,0)));
    assert(count == 5);
    return 0;
}
