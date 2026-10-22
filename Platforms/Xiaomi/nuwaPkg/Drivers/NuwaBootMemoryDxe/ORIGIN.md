# NuwaBootMemoryDxe provenance

Source: UniXNU/uefi-sheng@329b473df1e53a3bfa64b2b39b79670c4c02279b, Platforms/Xiaomi/shengPkg/Drivers/ShengBootMemoryDxe/.

The [pinned README](https://github.com/UniXNU/uefi-sheng/blob/329b473df1e53a3bfa64b2b39b79670c4c02279b/README.md#license) declares BSD-2-Clause for code outside the GPL directories, including these files. The upstream files contain no file-level copyright notice.

Applied the device repository 0003-initrd-publisher-only.patch first: retain read-only initrd BlockIo, remove DRAM discovery/GCD extension. Then rename the INF base/source/entry to NuwaBootMemoryDxe; retain the existing module GUID and protocol behavior. Source attribution comments and inherited SHENGMEM diagnostic strings remain. Runtime mapping and reservation lifetime on nuwa are unverified.
