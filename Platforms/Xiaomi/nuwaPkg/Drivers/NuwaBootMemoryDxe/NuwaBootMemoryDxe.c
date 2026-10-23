// SPDX-License-Identifier: BSD-2-Clause
/** @file
  Read-only initrd block publisher, adapted from pinned ShengBootMemoryDxe.

  Publish only a bounded, checksummed NUWA volume from the ABL initrd extent.
  The full FDT and initrd must lie in the existing reserved Kernel map region.
  No DRAM expansion or guessed Android addresses; no raw-initrd fallback.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryMapLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#include "NuwaVolume.h"

// A diagnostic build may hold the final checkpoint for a bounded recording window.
#ifndef NUWA_VOLUME_DIAGNOSTIC_PAUSE_US
#define NUWA_VOLUME_DIAGNOSTIC_PAUSE_US  0
#endif

STATIC
BOOLEAN
ReservedKernelRange (UINT64 Start, UINT64 Length)
{
  EFI_MEMORY_REGION_DESCRIPTOR *Map;
  UINT8 Count;
  UINTN Index;

  if ((Length == 0) || (Start > MAX_UINTN) || (Length > MAX_UINTN - Start)) {
    return FALSE;
  }
  GetMemoryMap (&Map, &Count);
  for (Index = 0; Index < Count; Index++) {
    if ((AsciiStrCmp (Map[Index].Name, "Kernel") == 0) &&
        (Map[Index].HobOption == AddMem) &&
        (Map[Index].ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) &&
        (Map[Index].MemoryType == EfiReservedMemoryType) &&
        (Map[Index].ArmAttributes == ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK) &&
        (Start >= Map[Index].Address) &&
        (Start - Map[Index].Address < Map[Index].Length) &&
        (Length <= Map[Index].Length - (Start - Map[Index].Address))) {
      return TRUE;
    }
  }
  return FALSE;
}

STATIC
BOOLEAN
VolumeCrc (CONST UINT8 *Data, UINTN Size, UINT32 *Result)
{
  return !EFI_ERROR (gBS->CalculateCrc32 ((VOID *)Data, Size, Result));
}

STATIC
UINT64
ReadCells (
  IN CONST VOID  *Data,
  IN UINT32      Cells
  )
{
  if (Cells == 2) {
    return Fdt64ToCpu (ReadUnaligned64 (Data));
  }

  return Fdt32ToCpu (ReadUnaligned32 (Data));
}

#define RAM_BLOCK_SIZE  512

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH          Vendor;
  EFI_DEVICE_PATH_PROTOCOL    End;
} RAM_BLOCK_DEVICE_PATH;
#pragma pack()

STATIC RAM_BLOCK_DEVICE_PATH  mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { (UINT8)sizeof (VENDOR_DEVICE_PATH), (UINT8)(sizeof (VENDOR_DEVICE_PATH) >> 8) } },
    { 0xA9C6827C, 0x1BD8, 0x410C, { 0x88, 0x5A, 0xC4, 0x65, 0x00, 0xF7, 0xD8, 0x36 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC UINT8                    *mRamBase;
STATIC EFI_BLOCK_IO_MEDIA       mMedia;

STATIC
EFI_STATUS
EFIAPI
RamBlockReset (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN BOOLEAN                ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockRead (
  IN  EFI_BLOCK_IO_PROTOCOL  *This,
  IN  UINT32                 MediaId,
  IN  EFI_LBA                Lba,
  IN  UINTN                  BufferSize,
  OUT VOID                   *Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (MediaId != mMedia.MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if ((BufferSize % RAM_BLOCK_SIZE) != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if ((Lba > mMedia.LastBlock) || ((BufferSize / RAM_BLOCK_SIZE) > (mMedia.LastBlock + 1 - Lba))) {
    return EFI_INVALID_PARAMETER;
  }

  CopyMem (Buffer, mRamBase + Lba * RAM_BLOCK_SIZE, BufferSize);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockWrite (
  IN EFI_BLOCK_IO_PROTOCOL  *This,
  IN UINT32                 MediaId,
  IN EFI_LBA                Lba,
  IN UINTN                  BufferSize,
  IN VOID                   *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
RamBlockFlush (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_BLOCK_IO_PROTOCOL  mBlockIo = {
  EFI_BLOCK_IO_PROTOCOL_REVISION,
  &mMedia,
  RamBlockReset,
  RamBlockRead,
  RamBlockWrite,
  RamBlockFlush
};

STATIC
EFI_STATUS
PublishInitrd (
  IN CONST VOID  *Fdt
  )
{
  EFI_STATUS  Status;
  INT32       Chosen;
  CONST VOID  *StartProp;
  CONST VOID  *EndProp;
  INT32       StartLength = 0;
  INT32       EndLength = 0;
  UINT64      Start;
  UINT64      End;
  EFI_HANDLE  Handle;
  UINTN       VolumeOffset;
  UINTN       VolumeSize;
  NUWA_VOLUME_RESULT VolumeResult;

  Chosen = FdtPathOffset (Fdt, "/chosen");
  if (Chosen < 0) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] no /chosen\n"));
    return EFI_NOT_FOUND;
  }

  StartProp = FdtGetProp (Fdt, Chosen, "linux,initrd-start", &StartLength);
  EndProp   = FdtGetProp (Fdt, Chosen, "linux,initrd-end", &EndLength);
  if ((StartProp == NULL) || (EndProp == NULL) ||
      ((StartLength != 4) && (StartLength != 8)) || ((EndLength != 4) && (EndLength != 8)))
  {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] missing or malformed initrd properties start-len=%d end-len=%d\n", StartLength, EndLength));
    return EFI_INVALID_PARAMETER;
  }

  Start = ReadCells (StartProp, StartLength / 4);
  End   = ReadCells (EndProp, EndLength / 4);
  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] chosen start=0x%lx end=0x%lx\n", Start, End));
  if ((End <= Start) || !ReservedKernelRange (Start, End - Start)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] rejected initrd extent\n"));
    return EFI_ACCESS_DENIED;
  }
  VolumeResult = NuwaFindVolume ((CONST UINT8 *)(UINTN)Start, (UINTN)(End - Start),
                                VolumeCrc, &VolumeOffset, &VolumeSize);
  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] envelope result=%u offset=0x%lx bytes=0x%lx\n",
          (UINT32)VolumeResult, (UINT64)VolumeOffset, (UINT64)VolumeSize));
  if (VolumeResult != NuwaVolumeFound) {
    return (VolumeResult == NuwaVolumeAbsent) ? EFI_NOT_FOUND : EFI_COMPROMISED_DATA;
  }
  // Kernel is already mapped and reserved by the unchanged platform memory map.
  // Keep the source reserved; publish just the validated subrange, read-only.
  Start += VolumeOffset;
  End = Start + VolumeSize;
  mRamBase                = (UINT8 *)(UINTN)Start;
  mMedia.MediaId          = 1;
  mMedia.RemovableMedia   = FALSE;
  mMedia.MediaPresent     = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly         = TRUE;
  mMedia.WriteCaching     = FALSE;
  mMedia.BlockSize        = RAM_BLOCK_SIZE;
  mMedia.IoAlign          = 1;
  mMedia.LastBlock        = (End - Start) / RAM_BLOCK_SIZE - 1;

  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  &mDevicePath,
                  &gEfiBlockIoProtocolGuid,
                  &mBlockIo,
                  NULL
                  );
  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] block device 0x%lx size 0x%lx %r\n", Start, End - Start, Status));
  return Status;
}

STATIC
EFI_STATUS
DiscoverInitrd (VOID)
{
  EFI_HOB_GUID_TYPE  *Hob;
  UINT64             Dtb;

  Hob = GetFirstGuidHob (&gSiliciumEntryArgHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) < sizeof (UINT64))) {
    DEBUG ((DEBUG_ERROR, "SHENGMEM: no entry argument HOB\n"));
    return EFI_NOT_FOUND;
  }

  Dtb = ReadUnaligned64 (GET_GUID_HOB_DATA (Hob));
  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] entry dtb=0x%lx\n", Dtb));
  if ((Dtb == 0) || ((Dtb & 3) != 0)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] invalid dtb pointer\n"));
    return EFI_INVALID_PARAMETER;
  }
  // Check the mapping before touching the FDT header, then bound its whole extent.
  if (!ReservedKernelRange (Dtb, 40)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] dtb header outside reserved Kernel map\n"));
    return EFI_ACCESS_DENIED;
  }
  if ((FdtCheckHeader ((VOID *)(UINTN)Dtb) != 0) ||
      (FdtTotalSize ((VOID *)(UINTN)Dtb) < 40)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] malformed dtb header\n"));
    return EFI_COMPROMISED_DATA;
  }
  if (!ReservedKernelRange (Dtb, FdtTotalSize ((VOID *)(UINTN)Dtb))) {
    DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] dtb extent outside reserved Kernel map bytes=0x%x\n",
            (UINT32)FdtTotalSize ((VOID *)(UINTN)Dtb)));
    return EFI_ACCESS_DENIED;
  }

  DEBUG ((DEBUG_ERROR, "SHENGMEM: device tree at 0x%lx, %u bytes\n", Dtb, (UINT32)FdtTotalSize ((VOID *)(UINTN)Dtb)));

  return PublishInitrd ((VOID *)(UINTN)Dtb);
}

EFI_STATUS
EFIAPI
NuwaBootMemoryDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;

  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] publisher enter\n"));
  Status = DiscoverInitrd ();
  DEBUG ((DEBUG_ERROR, "[NUWA-VOLUME] publisher exit status=%r\n", Status));
#if NUWA_VOLUME_DIAGNOSTIC_PAUSE_US > 0
  // This hold is opt-in in the separate diagnostic profile, never the normal build.
  gBS->Stall (NUWA_VOLUME_DIAGNOSTIC_PAUSE_US);
#endif
  return Status;
}
