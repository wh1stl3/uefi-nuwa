// SPDX-License-Identifier: BSD-2-Clause
/** Read-only OS-entry input evidence. No window is allocated or reserved.
  The memory-map buffer itself is allocated before the snapshot and then freed.
  This opt-in diagnostic runs once at ReadyToBoot, after ordinary DXE dispatch.
**/
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "NuwaMemorySummary.h"

STATIC BOOLEAN mDone;

STATIC VOID EFIAPI
AuditReadyToBoot (IN EFI_EVENT Event, IN VOID *Context)
{
  EFI_STATUS Status;
  EFI_MEMORY_DESCRIPTOR *Map;
  UINTN Size;
  UINTN Capacity;
  UINTN Key;
  UINTN DescriptorSize;
  UINT32 Version;
  UINTN Attempt;
  UINTN Index;
  UINT64 CurrentEl;
  UINT64 Mpidr;
  UINT64 CounterFrequency;
  NUWA_AUDIT_RANGE Ranges[NUWA_AUDIT_MAX_RANGES];
  NUWA_AUDIT_SUMMARY Summary;

  if (mDone) {
    return;
  }
  mDone = TRUE;
  gBS->CloseEvent (Event);
  __asm__ __volatile__ ("mrs %0, CurrentEL" : "=r" (CurrentEl));
  __asm__ __volatile__ ("mrs %0, mpidr_el1" : "=r" (Mpidr));
  __asm__ __volatile__ ("mrs %0, cntfrq_el0" : "=r" (CounterFrequency));
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] ready el=%lu mpidr=0x%lx cntfrq=%lu\n",
          CurrentEl >> 2, Mpidr, CounterFrequency));
  Size = 0;
  DescriptorSize = 0;
  Map = NULL;
  Status = gBS->GetMemoryMap (&Size, NULL, &Key, &DescriptorSize, &Version);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] sizing failed %r\n", Status));
    return;
  }
  Status = EFI_BUFFER_TOO_SMALL;
  for (Attempt = 0; Attempt < 4; Attempt++) {
    if (Size > 1048576 - 16 * sizeof (EFI_MEMORY_DESCRIPTOR)) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }
    Capacity = Size + 16 * sizeof (EFI_MEMORY_DESCRIPTOR);
    Map = AllocatePool (Capacity);
    if (Map == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      break;
    }
    Size = Capacity;
    Status = gBS->GetMemoryMap (&Size, Map, &Key, &DescriptorSize, &Version);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      break;
    }
    FreePool (Map);
    Map = NULL;
  }
  if (EFI_ERROR (Status)) {
    if (Map != NULL) {
      FreePool (Map);
    }
    DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] snapshot failed %r\n", Status));
    return;
  }
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] map bytes=%lu stride=%lu version=%u\n",
          (UINT64)Size, (UINT64)DescriptorSize, Version));
  Status = NuwaSummarizeMemory (Map, Size, DescriptorSize, Ranges, &Summary);
  FreePool (Map);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] invalid or excessive map %r\n", Status));
    return;
  }
  for (Index = 0; Index < Summary.MergedCount; Index++) {
    DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] conventional %u start=0x%lx end=0x%lx attr=0x%lx\n",
            (UINT32)Index, Ranges[Index].Start, Ranges[Index].End, Ranges[Index].Attributes));
  }
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] summary descriptors=%u conventional=%u merged=%u total=0x%lx\n",
          (UINT32)Summary.DescriptorCount, (UINT32)Summary.ConventionalCount,
          (UINT32)Summary.MergedCount, Summary.TotalBytes));
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] largest base=0x%lx bytes=0x%lx wb-base=0x%lx wb-bytes=0x%lx\n",
          Summary.LargestBase, Summary.LargestBytes, Summary.LargestWbBase, Summary.LargestWbBytes));
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] snapshot only; no window allocated or reserved\n"));
  gBS->Stall (10000000);
}

EFI_STATUS EFIAPI
NuwaBootAuditDxeEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_EVENT Event;
  EFI_STATUS Status;
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, AuditReadyToBoot,
                              NULL, &gEfiEventReadyToBootGuid, &Event);
  DEBUG ((DEBUG_ERROR, "[NUWA-BOOT-AUDIT] registered %r\n", Status));
  return Status;
}
