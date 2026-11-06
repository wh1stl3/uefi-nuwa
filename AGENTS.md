# Agent instructions — uefi-nuwa

## 治理规则（必读）
1. 长期规则：`../../docs/governance-2026-11-06.md`（11 条：官方优先/两仓制/结构/流程/上游同步/脱敏/媒体/Apple 合规/验证精简/WSL 优先/磁盘治理）。
2. AI 分工：`../../docs/ai-orchestration-2026-11-06.md`。
3. nuwa 路线背景：`../../docs/nuwa-ios17-sheng-migration-2026-11-06.md`。

## 职责
维护 nuwa（SM8550）的 Mu-Silicium UEFI 集成：内存保留、平台初始化、固件打包、硬件交接。
本仓派生自 UniXNU/uefi-sheng@66421cb（2026-11-06）；sheng 的板级值仅作同 SoC 参考，禁止照抄（MMIO/IRQ/面板/电源须 nuwa 证据）。

## 必读（官方文档）
1. README.md、Building.md、`Platforms/Xiaomi/shengPkg/PORTING.md`（作为 nuwaPkg 结构参考）。
2. [firmware integration](https://unixnu.com/porting/uefi-firmware)、[SM8550](https://unixnu.com/porting/sm8550)、[porting](https://unixnu.com/porting/)、[repository specification](https://unixnu.com/porting/device-repository)、[component catalog](https://unixnu.com/reference/components)、[device-tree specification](https://unixnu.com/porting/device-tree)。
3. 所选组件的 README、构建接口与实现；[payload](https://unixnu.com/concepts/payload)、[building](https://unixnu.com/building)。

## 硬边界
- 禁止提交 OEM 二进制、设备标识符、生成镜像；vendor 输入放 `.cache/firmware/` 不入库（治理 §6/§7）。
- 内存大小/基址修改与 `*_verified=true` 须用户当轮确认。
- 提交格式 `[uefi-nuwa][<area>]` + Change/Evidence/Risk；不 force push（main 已设保护）。
