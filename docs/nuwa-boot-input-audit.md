# Nuwa OS-entry input audit

NuwaBootAuditDxe is selected only in the boot-input-audit diagnostic profile.
It registers a one-shot ReadyToBoot notification, reads CurrentEL, MPIDR_EL1 and
CNTFRQ_EL0, and obtains a bounded GetMemoryMap snapshot. It does not reserve an
OS window, write MMIO, change translation policy, call PSCI or exit boot services.

The summary validates descriptor stride, extent arithmetic and a bounded range
count, sorts conventional descriptors, and merges only physically adjacent
ranges with identical attributes. It prints all merged ranges, total conventional
bytes, and largest conventional/write-back runs. Overflow, overlap and excessive
input fail explicitly instead of producing a partial success. The snapshot
includes its temporary map buffer; it is not a promise of later allocation or
ownership. A separate actual allocation/target ADT review is still required.

After a successful dump it holds ten seconds for recording, then normal boot
continues to the known configuration probe. No physical addresses or capacity
settings are added to the platform map. Host tests execute the exact summary
header on unsorted, adjacent, gapped, attribute-mismatched, extended-stride,
overlapping and overflowing maps. They do not validate the ReadyToBoot event,
register reads or real memory ownership.

## Execution status (2026-10-24)

The a2dc8195 input-audit trial stopped at an early HOB check, before this ReadyToBoot callback. The subsequent 2273bdec trial localized the first observed failure to after-mmu. Cache candidate 26ef8efd now has a user report of the UniXNU menu, but no ReadyToBoot dump or allocation proof has been received. These later observations do not qualify an OS memory window. Candidate and trial evidence remain in the device repository; see nuwa-unixnu.md for the current handoff.
