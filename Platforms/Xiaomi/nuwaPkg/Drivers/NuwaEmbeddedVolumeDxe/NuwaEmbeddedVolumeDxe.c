// SPDX-License-Identifier: BSD-2-Clause
/** Configuration-only FV probe; independent of ABL's chosen initrd.
  BlockIo callbacks reuse the pinned nuwa publisher. The FV read allocates
  boot-services pool; retain it on success for the lifetime of the block device.
  This driver is included only by the embedded-probe build profile.
**/
#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#define PROBE_BYTES (4 * 1024 * 1024)
STATIC EFI_GUID mProbeFile = { 0x3F056471, 0xED81, 0x4B72, {0xB1,0x50,0xF0,0x32,0x6E,0x39,0x12,0x86} };
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
    { 0xD6366842, 0x3C85, 0x42ED, { 0x88, 0x5A, 0xC4, 0x65, 0x00, 0xF7, 0xD8, 0x36 } }
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

EFI_STATUS
EFIAPI
NuwaEmbeddedVolumeDxeEntry (IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  VOID *Data;
  UINTN Size;
  EFI_HANDLE Handle;
  UINT32 Crc;

  Data = NULL;
  Size = 0;
  DEBUG ((DEBUG_ERROR, "[NUWA-FV-PROBE] enter\n"));
  Status = GetSectionFromAnyFv (&mProbeFile, EFI_SECTION_RAW, 0, &Data, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "[NUWA-FV-PROBE] read resource %r\n", Status));
    return Status;
  }
  // The build pins every byte of this configuration-only FAT16 image.
  // Reject a wrong resource rather than treating arbitrary FV data as a disk.
  if ((Data == NULL) || (Size != PROBE_BYTES) ||
      (((UINT8 *)Data)[510] != 0x55) || (((UINT8 *)Data)[511] != 0xAA)) {
    if (Data != NULL) {
      FreePool (Data);
    }
    return EFI_COMPROMISED_DATA;
  }
  Status = gBS->CalculateCrc32 (Data, Size, &Crc);
  if (EFI_ERROR (Status) || (Crc != 0x54FE5070)) {
    FreePool (Data);
    return EFI_COMPROMISED_DATA;
  }
  mRamBase = Data;
  mMedia.MediaId = 1;
  mMedia.RemovableMedia = FALSE;
  mMedia.MediaPresent = TRUE;
  mMedia.LogicalPartition = FALSE;
  mMedia.ReadOnly = TRUE;
  mMedia.WriteCaching = FALSE;
  mMedia.BlockSize = RAM_BLOCK_SIZE;
  mMedia.IoAlign = 1;
  mMedia.LastBlock = Size / RAM_BLOCK_SIZE - 1;
  Handle = NULL;
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle,
    &gEfiDevicePathProtocolGuid, &mDevicePath,
    &gEfiBlockIoProtocolGuid, &mBlockIo, NULL);
  if (EFI_ERROR (Status)) {
    mRamBase = NULL;
    FreePool (Data);
  }
  DEBUG ((DEBUG_ERROR, "[NUWA-FV-PROBE] block device bytes=0x%lx status=%r\n", (UINT64)Size, Status));
  gBS->Stall (5000000);
  return Status;
}
