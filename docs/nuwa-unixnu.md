# UniXNU nuwa firmware source

Tested source baseline: Project-Silicium/Mu-Silicium@6c6755012728c9f7de748228758bad94d1e9a660. The inherited main branch is preserved; codex/nuwa-bringup is the device development branch.

Device integration lives at https://github.com/wh1stl3/UniXNU-nuwa. This fork owns the BootShim size correction, nuwa DSC/FDF/package wiring, SEC x0/HOB handoff, and the read-only NuwaBootMemoryDxe initrd publisher. UniXNUPkg and DarwinBootPkg remain external packages, staged under .unixnu-packages by the device build driver. No full UniXNU tree is vendored.

Source migration only: the dedicated fork has not been built or tested on hardware. Historical CI39764322744 used equivalent source patches before the publisher rename; v4 boot acceptance/black screen did not prove SEC/DXE/BDS execution. Keep memory/config maps and all submodule pins at the tested baseline. Do not import sheng DRAM policy.

Inherited Actions are disabled to prevent image generation during source provisioning. Firmware builds remain explicitly dispatched from the device repository, subject to its AGENTS.md and approved scope.
