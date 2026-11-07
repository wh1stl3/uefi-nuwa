# uefi-nuwa

小米 13 Pro（nuwa / SM8550）的 Mu-Silicium UEFI 固件仓。

- 基线：UniXNU/uefi-sheng@66421cb7bb5694843aec8de1ca3d1de4c20cca38（2026-11-06 派生，已去除 sheng 历史）。

## 当前进度（2026-11-07）
- 阶段：**官方 steps 第 5 步（UEFI 集成）进行中**。
- 已完成候选（未提交，待主控独立审查）：
  - `Platforms/Xiaomi/nuwaPkg/nuwa.{dsc,fdf}` 加入 required-integration 模块：`UniXNU` app、`UniXNUDxe`、`UniXNURamDiskDxe`、`DarwinBoot`、`AdvancedLogDxe`；DSC 补 `UniXNULib`/`FdtLib`/`HobLib` 与 `AdvancedLoggerAccessLib`/`QrEncoderLib` 库映射；`PcdUniXNUDeviceModel=nuwa`。
  - `DiskIoDxe`/`PartitionDxe`/`FAT`/`UnicodeCollation` 已在 `Include/DXE.inc` 且经 `SiliciumPkg.dsc.inc` 进入 DSC `[Components]`（已核对，无改动）。
  - 5 个源码 submodule gitlink 已从上游 uefi-sheng@66421cb 精确恢复（见下方）。`Binaries`（OEM）不入库。
- 未验证/阻塞：
  - **内存地址未验证**：现行 FD `0xA7100000`、DXE heap 与历史 ramoops/iomem reserved 窗口相交；`RAM Partition→HOB→EFI map→DarwinBoot` 窗口证据未采集前，禁止实机交接放行，所有地址/size/verified 保持原值不变。
  - **vendor/OEM 输入缺失**：`Binaries/nuwa/` 未提供（放 `.cache/firmware/` 不入库），固件无法本地编译；submodule 内容（Common/Mu、Mu_Basecore 等）尚未检出。
  - **Advanced Logger 生产者未接线**：`AdvancedLogDxe` 的 Depex 为 `gAdvancedLoggerProtocolGuid`；当前 Silicium DSC 未产出该协议也未把 DebugLib 路由进 Advanced Logger 缓冲，且 mu_plus release/202608 的 DXE 协议生产者仅注册于 X64。接好前 Tools → Advanced Logger 不会出现/会显示 "no log"（不得以空 logger 冒充采集）。

## Submodule pins（源码，精确恢复自上游 uefi-sheng@66421cb7bb5694843aec8de1ca3d1de4c20cca38）
| 路径 | gitlink SHA |
| --- | --- |
| Common/Mu（mu_plus） | b8d46c71610318e36a661819e1e3c0b8bdda413a |
| Common/Mu_OEM_Sample | b70b4b7bb02c43bf9db2172dfb3ed0457e2361d6 |
| Mu_Basecore | cfb93724a06ff86ff5ba971413368a7809ee42a5 |
| Silicium-ACPI | 2d465ad77c25de908f0ec7bdfcb57f865b61233c |
| Silicon/Silicium/OpensslPkg/Library/OpensslLib/openssl | 67b5686b4419b4cb8caa502711c41815f5279751 |

`Binaries`（Device-Binaries，OEM）不提交，vendor 输入放 `.cache/firmware/`。

## 约定
规则见 `../docs/governance-2026-11-06.md`（本机共享）。OEM 二进制与 vendor 输入放 `.cache/firmware/` 不入库；含 Apple 组件产物不上传（治理 §7）。
