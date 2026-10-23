# Nuwa volume transport v1

This is a candidate transport for a read-only FAT volume inside the Android
generic ramdisk. The installed ABL may prepend vendor bytes and append bootconfig;
the entire `/chosen/linux,initrd-*` extent is therefore not a disk image.

The publisher accepts only an FDT and initrd wholly contained in the existing
`Kernel` entry of MemoryMapLib, with AddMem, system-memory, reserved-memory and
write-back attributes. No addresses or sizes in the platform map are changed.
The source stays reserved for the lifetime of BlockIo. A range outside this map
entry fails closed; the driver does not expand mappings to accommodate it.

Within that validated extent it scans for this exact 64-byte header, at any byte
alignment. This is not a global RAM search or FAT signature search.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 16 | `4e5557412d564f4ca3719c4e82d650b9` |
| 16 | 4 | Little-endian version, 1 |
| 20 | 4 | Little-endian header length, 64 |
| 24 | 8 | Little-endian payload length, positive and divisible by 512 |
| 32 | 4 | Standard IEEE CRC32 of the payload |
| 36 | 28 | Zero reserved bytes |
| 64 | length | Complete volume bytes |

The payload must be wholly inside the chosen extent. Unknown versions, malformed
headers, CRC mismatches and a second envelope outside the first payload fail
closed. Magic inside the payload is ordinary file data. CRC detects corruption;
it provides no authenticity guarantee. Host artifact provenance uses SHA256.
The driver publishes only the validated subrange, with the existing read-only
BlockIo callbacks; it never rounds up an extent or falls back to the raw initrd.

`tests/test_nuwa_volume.py` compiles the exact C parser and executes it on a host,
including unaligned prefixes, bootconfig-like suffixes, every truncation, oversized
lengths, corruption, duplicate envelopes and randomized placements. These tests
do not demonstrate an actual ABL handoff, device mapping or an XNU boot.

Runtime checkpoints are `[NUWA-VOLUME] chosen`, `envelope result`, and
`block device ... Success`. Result 0 means absent, 1 found, 2 invalid, 3 duplicate.
The first device payload should be a configuration-only FAT probe. A probe menu
is volume-discovery evidence, not XNU execution.
