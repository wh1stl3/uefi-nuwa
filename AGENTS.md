# Agent instructions — uefi-nuwa

## 治理规则（必读）
1. 长期规则：`../../docs/governance-2026-11-06.md`（12 条，v2 对齐官方 format 2：官方优先/三仓+组件仓/结构/流程/上游同步/脱敏/Apple 合规/验证精简/Linux 优先/磁盘治理/交接/违例判定）。
2. AI 分工：`../../docs/ai-orchestration-2026-11-06.md`。
3. nuwa 路线背景：`../../docs/nuwa-ios17-sheng-migration-2026-11-06.md`。

## 职责
维护 nuwa（SM8550）的 Mu-Silicium UEFI 集成：内存保留、平台初始化、固件打包、硬件交接。
本仓派生自 UniXNU/uefi-sheng@66421cb（2026-11-06）；sheng 的板级值仅作同 SoC 参考，禁止照抄（MMIO/IRQ/面板/电源须 nuwa 证据）。

## 必读（官方文档）
1. README.md、Building.md、`Platforms/Xiaomi/shengPkg/PORTING.md`（作为 nuwaPkg 结构参考）。
2. [firmware integration](https://unixnu.com/zh/porting/uefi-firmware)、[SoC support](https://unixnu.com/zh/porting/soc-support)、[porting](https://unixnu.com/zh/porting/)、[repository specification](https://unixnu.com/zh/porting/device-repository)、[component catalog](https://unixnu.com/zh/reference/components)、[device-tree specification](https://unixnu.com/zh/porting/device-tree)、[bring-up](https://unixnu.com/zh/porting/bring-up)。
3. 所选组件的 README、构建接口与实现；[payload](https://unixnu.com/zh/concepts/payload)、[building](https://unixnu.com/zh/building)。

## 硬边界
- 禁止提交 OEM 二进制、设备标识符、生成镜像；vendor 输入放 `.cache/firmware/` 不入库（治理 §6/§7）。
- 内存大小/基址修改与 `*_verified=true` 须用户当轮确认。
- DSC/FDF 必须含 `UniXNUPkg/Application/UniXNU`、`UniXNUPkg/Driver/UniXNUDxe`、`DarwinBootPkg/Driver/DarwinBoot`；用 FDT initrd 负载时再加 `UniXNURamDiskDxe` + `DiskIoDxe` + FAT。
- 提交格式 `[uefi-nuwa][<area>]` + Change/Evidence/Risk；不 force push（main 已设保护）。
