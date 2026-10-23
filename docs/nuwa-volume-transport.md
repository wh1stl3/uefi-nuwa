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

## Publisher failure status

The publisher now returns its actual failure to the DXE dispatcher. Missing HOB,
chosen node or envelope returns EFI_NOT_FOUND; invalid pointer/property encoding
returns EFI_INVALID_PARAMETER; an extent outside the unchanged reserved Kernel
map returns EFI_ACCESS_DENIED; malformed FDT/envelope returns
EFI_COMPROMISED_DATA. Protocol-install status is propagated unchanged.
Each rejection logs its stage, with entry DTB and chosen bounds when available.

The normal build has no delay. An explicitly selected diagnostic build can set
NUWA_VOLUME_DIAGNOSTIC_PAUSE_US to 5000000 to hold the final debug checkpoint
for five seconds before returning to DXE. This needs a fresh recording gate
before any device trial and does not establish a reliable display channel.

`python3 -B -m unittest discover -s tests -v` compiles both the actual parser
and publisher with host doubles and UBSan. Publisher checks include rejecting
unmapped FDT pointers before any header read, malformed/absent input, rejected
initrd bounds before CRC, successful read-only publication, and propagation of
protocol-install failure. These are host checks, not device execution.

## Embedded configuration probe (separate profile)

NuwaEmbeddedVolumeDxe is an opt-in diagnostic module, not included by the normal
nuwa DSC/FDF. It reads RAW file 3F056471-ED81-4B72-B150-F0326E391286 from the FV
using DxeServicesLib. The exact 4MiB FAT16 resource (SHA256
c097413db551aac705e530cef697fdbc54ae9a9c4ea8ae86be175ba5bdd15796)
has only EFI/UniXNU/unixnu.toml and an exit action. Size, boot signature and
CRC32 54fe5070 are checked before read-only BlockIo publication. Failed reads,
invalid resources and protocol-install failure do not retain a buffer; successful
publication retains the boot-services pool until firmware exits boot services.
The BlockIo callbacks are reused from this repository's BSD-2-Clause publisher.

This verifies a separate FAT/config discovery route without relying on ABL
passing the downloaded boot v4 ramdisk through chosen. It does not solve transport
of a full XNU payload. Decompressed FV and pool use increase; complete build and hardware behavior were unverified at the time of this implementation note. No map, address or verification flag changes.

## Subsequent evidence (2026-10-24)

The87b14624 embedded-probe trial produced photographed publication Success and the unique configuration menu; this validates the small configuration discovery route on that trial. The later26ef8efd cache candidate has a user text report of reaching the menu, with raw cache-stage images pending. Both trials recovered Android with unchanged three boot-partition hashes. Neither carries or executes XNU/ADT, and neither qualifies full-payload transport. See the device repository current STATUS and mainline handoff linked in nuwa-unixnu.md.
