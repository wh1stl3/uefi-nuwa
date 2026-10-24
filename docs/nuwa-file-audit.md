# Opt-in FV file SHA diagnostic

NuwaFileAuditDxe is source-only until an explicit device-repository diagnostic
profile selects it. Normal nuwa DSC/FDF selections are unchanged. It does not
disable image authentication or change trusted-key variables. The preceding
external EFI application's menu returned Access Denied; FV integration changes
the diagnostic's dispatch location, not the filesystem payload's trust status.

A one-shot ReadyToBoot callback enumerates at most 64 filesystem handles and
opens only EFI/UniXNU/Payloads/READTEST.BIN with EFI_FILE_MODE_READ. A missing
file is skipped; other filesystem errors are propagated. Duplicate target files
fail closed before hashing. The fixed file is 58,335,232 bytes, SHA256
eb66542cc9b6c34f369c07229900408ab67288152f561be63fa6061efce9ffca.
Its analytical Mach-O bytes are data only and are never executed.

GetInfo metadata is bounded to 4 KiB and validated before normal AllocatePool.
Reads are bounded to 1 MiB and 4096 calls, allow positive short reads and refuse
early EOF or excessive returned lengths. An extra byte request verifies EOF.
The existing BaseCryptLib implementation hashes the full file; explicit screen
markers show enter, unique file, read progress, SHA, PASS/exit status. All pools
and opened file/root handles are released on ordinary return paths. Results are
held for 30 seconds. A blocked underlying synchronous Read is not preempted.

No writes, kernel jump, ExitBootServices, board values, Heap/HOB changes, or
translation changes are introduced. The callback performs a temporary ordinary
pool allocation, not an OS window reservation. Real firmware dispatch, console
visibility, actual library linkage and nuwa file verification require artifact
inspection and a separately captured device trial.

Offline evidence in the device repository: tests/fv_file_audit_host.cpp compiles
the actual C source into an AArch64 software protocol fixture. Eighteen
file/volume/crypto cases and a one-shot callback case passed with balanced pool
and file cleanup. The fixture's crypto callback uses the previously pinned
NativeAdreno SHA implementation; the target firmware instead links its existing
BaseCryptLib, which must be checked in the actual build report. This test is not
real hardware or a complete target crypto-library execution test.
