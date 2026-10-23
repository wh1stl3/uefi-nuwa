# UniXNU nuwa firmware source

Tested source baseline: Project-Silicium/Mu-Silicium@6c6755012728c9f7de748228758bad94d1e9a660. The inherited main branch is preserved; codex/nuwa-bringup was the initial development branch; current diagnostic integration uses codex/nuwa-sec-handoff-trace.

Device integration lives at https://github.com/wh1stl3/UniXNU-nuwa. This fork owns the BootShim size correction, nuwa DSC/FDF/package wiring, SEC x0/HOB handoff, and the read-only NuwaBootMemoryDxe initrd publisher. UniXNUPkg and DarwinBootPkg remain external packages, staged under .unixnu-packages by the device build driver. No full UniXNU tree is vendored.

Historical source-provisioning status (superseded by the update below): the dedicated fork initially had not been built or tested on hardware. Historical CI39764322744 used equivalent source patches before the publisher rename; v4 boot acceptance/black screen did not prove SEC/DXE/BDS execution. Keep memory/config maps and all submodule pins at the tested baseline. Do not import sheng DRAM policy.

Inherited Actions are disabled to prevent image generation during source provisioning. Firmware builds remain explicitly dispatched from the device repository, subject to its AGENTS.md and approved scope.

## Earlier cache-candidate trial (2026-10-24)

The device repository is the authority for candidate patches and trial evidence. CI40005057158 built fixed firmware 6176a822e66b846620b5bcd2739a0666814ee663 plus the device-owned diagnostic patch chain including 0019. The cache candidate image is 26ef8efdc034f726df23f7bb2998d9e980e52e706e914ba9a415ca088dba2051. This documentation update does not change that source pin or promote candidate patches into normal firmware.

After a fresh recording confirmation and a single temporary boot, the user reported reaching the UniXNU menu. Android recovery completed with boot_completed=1 and unchanged boot_a/vendor_boot_a/init_boot_a SHA256. Five subsequent user screenshots now show the cache invalidate marker, after-mmu ok (count=84), memory status Success, ReadyToBoot and the unique configuration menu. This trial passes those stages; root-cause uniqueness, repeatability and XNU/iOS execution remain unverified. Heap, memory-map values and bad-entry protection were retained. No flash, erase or slot change occurred.

See the [current device handoff](https://github.com/wh1stl3/UniXNU-nuwa/blob/main/docs/operations/2026-10-23-mainline-handoff.md) and [trial record at 747d8fc](https://github.com/wh1stl3/UniXNU-nuwa/blob/747d8fcc7368778a4af6ff49bf3d845e5154cf2e/docs/operations/2026-10-24-hob-cache-handoff-trial.md). Do not reuse old preflight files or recording consent for another boot.

The user reports the trial ended at a charging screen. The supplied final image shows Not Found and USB fallback, consistent with the configuration-only probe timeout=60/action=exit. Charging itself is a user observation, not photographed here. Raw images and SHA256 are archived in the device repository reports/hob-cache-handoff-trial/2026-10-24/screen-observation.json.

## All reviewed upstream updates merged (2026-10-24)

The user explicitly requested merging all reviewed updates. The base fork now contains the real two-parent merge e553e467ebfe7642e9f483d3977a57066c6d1aef (upstream 1a01c2af plus three preserved nuwa workflows), including all 21 fixed driver/tool submodules. The device repository's default firmware build profile is upstream-merged. This profile retains firmware 6176a82 and the full 0019 cache/protection patch chain; it supplies the newly merged external UniXNU/DarwinBoot packages.

CI40014018074/source device 5abdde1 completed the full firmware build and 697 artifact checks. FD SHA256 is 2d606985ad768dcfaab66c2b28073c0a4c2820126b42b23286623fdc450e848f (3MiB). The entire SEC remains byte-identical to the earlier physically tested cache candidate, preserving its Heap/memory table and HOB checks/maintenance. Actual DSB/IVAC/DSB/ISB and checkpoint/failure paths were re-audited. The new upper packages are built, but this new FD has not been booted on nuwa.

The Darwin22 source-kernel integration runs separately. Its first build exposed an omitted prerequisite for the new sandbox property consumer; the real upstream grant helper patch was added and all 64 ordered patches pass the context check. Retry CI40015845083 uses device 6070627; consult the device handoff for the final result. No sheng touch/CPU/storage values or Data-reset actions were adopted for nuwa. No physical test, flash or board-memory change occurred during this merge.
