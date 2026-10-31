# ChoralRoot rhythmic arpeggiator patterns (experimental)

CoralFlower's `cf_arp.c` now includes ChoralRoot's 12 designed rhythmic phrases, ported from `Quixotic7/ChoralRootFM1/firmware/src/cr_engine.c`. They are **not** reverse-engineered Orchid factory patterns. Original 9 arp modes retain their numeric values; the 12 new modes are appended as Pattern pulse, alberti, waltz, cascade, updown, syncopate, gallop, skip, ballad, stride, drive and ladder.

The packed steps contain 1-based chord-tone indices (0 = rest) and high-nibble accents. This initial port uses the original note order and rests with CoralFlower's existing RATE, GATE, SWING and OCTAVE. **Accent velocities are not yet applied**; the high nibble is preserved for a follow-up change. When a phrase references a note beyond the available chord tones, the index wraps over the expanded octave range. The new patterns are selectable in the existing ARP MODE UI; a dedicated Pattern parameter is not yet added.

This implementation has not been compiled or hardware-tested. Review playback behavior, save/load and UI labels before flashing.
