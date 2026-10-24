// SPDX-License-Identifier: BSD-2-Clause
// Opt-in FV diagnostic. Read/hash data only, never execute the Mach-O file.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Guid/FileInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/BaseCryptLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#define NUWA_FILE_BYTES 58335232U
#define NUWA_MAX_FS 64U
#define NUWA_MAX_READS 4096U
STATIC CONST UINT8 Expected[32] = {
  0xeb,0x66,0x54,0x2c,0xc9,0xb6,0xc3,0x4f,0x36,0x9c,0x07,0x22,0x99,0x00,0x40,0x8a,
  0xb6,0x72,0x88,0x15,0x2f,0x56,0x1b,0xe6,0x3f,0xa6,0x06,0x1e,0xfc,0xe9,0xff,0xca
};
STATIC BOOLEAN Done;

STATIC VOID Message (CONST CHAR16 *Text) {
  Print (L"[NUWA-FV-FILE] %s\r\n", Text);
}

STATIC EFI_STATUS ReadAndHash (EFI_FILE_PROTOCOL *File) {
  EFI_STATUS Status;
  EFI_FILE_INFO *Info = NULL;
  UINT8 *Buffer = NULL;
  UINTN InfoBytes = 0, Capacity, Total = 0, Calls = 0, Got, Wanted, Index;
  UINT8 Digest[32], Extra;
  CHAR16 Hash[65];
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoBytes, NULL);
  if ((Status != EFI_BUFFER_TOO_SMALL) || (InfoBytes < SIZE_OF_EFI_FILE_INFO) || (InfoBytes > 4096)) {
    return EFI_COMPROMISED_DATA;
  }
  Capacity = InfoBytes;
  Info = AllocatePool (Capacity);
  if (Info == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = File->GetInfo (File, &gEfiFileInfoGuid, &InfoBytes, Info);
  if (EFI_ERROR (Status)) { goto Done; }
  if ((InfoBytes > Capacity) || (InfoBytes < SIZE_OF_EFI_FILE_INFO) ||
      (Info->Size < SIZE_OF_EFI_FILE_INFO) || (Info->Size > InfoBytes) ||
      ((Info->Attribute & EFI_FILE_DIRECTORY) != 0) || (Info->FileSize != NUWA_FILE_BYTES)) {
    Status = EFI_COMPROMISED_DATA; goto Done;
  }
  Buffer = AllocatePool (NUWA_FILE_BYTES);
  if (Buffer == NULL) { Status = EFI_OUT_OF_RESOURCES; goto Done; }
  Message (L"allocated; reading fixed data file (no execution)");
  while (Total < NUWA_FILE_BYTES) {
    if (++Calls > NUWA_MAX_READS) { Status = EFI_ABORTED; goto Done; }
    Wanted = NUWA_FILE_BYTES - Total;
    if (Wanted > 1048576) { Wanted = 1048576; }
    Got = Wanted;
    Status = File->Read (File, &Got, Buffer + Total);
    if (EFI_ERROR (Status)) { goto Done; }
    if ((Got == 0) || (Got > Wanted)) { Status = EFI_COMPROMISED_DATA; goto Done; }
    Total += Got;
    if (((Total % 8388608) == 0) || (Total == NUWA_FILE_BYTES)) {
      Print (L"[NUWA-FV-FILE] read bytes=0x%lx\r\n", (UINT64)Total);
    }
  }
  Got = 1;
  Status = File->Read (File, &Got, &Extra);
  if (EFI_ERROR (Status)) { goto Done; }
  if (Got != 0) { Status = EFI_COMPROMISED_DATA; goto Done; }
  if (!Sha256HashAll (Buffer, Total, Digest)) { Status = EFI_DEVICE_ERROR; goto Done; }
  for (Index = 0; Index < 32; Index++) {
    Hash[2*Index] = L"0123456789abcdef"[Digest[Index] >> 4];
    Hash[2*Index+1] = L"0123456789abcdef"[Digest[Index] & 15];
  }
  Hash[64] = 0;
  Print (L"[NUWA-FV-FILE] actual sha256=%s\r\n", Hash);
  if (CompareMem (Digest, Expected, sizeof (Expected)) != 0) { Status = EFI_CRC_ERROR; goto Done; }
  Message (L"PASS exact length, EOF and SHA256");
  Status = EFI_SUCCESS;
Done:
  Print (L"[NUWA-FV-FILE] final bytes=0x%lx calls=%lu status=%r\r\n", (UINT64)Total, (UINT64)Calls, Status);
  if (Buffer != NULL) { FreePool (Buffer); }
  FreePool (Info);
  return Status;
}

STATIC EFI_STATUS AuditVolumes (VOID) {
  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0, Index, Matches = 0;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Root, *File, *Selected = NULL;
  EFI_STATUS Status;
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) { return Status; }
  if ((Count > NUWA_MAX_FS) || (Count == 0)) {
    Status = EFI_BAD_BUFFER_SIZE; goto Done;
  }
  for (Index = 0; Index < Count; Index++) {
    Root = NULL; File = NULL;
    Status = gBS->HandleProtocol (Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) { goto Done; }
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) { goto Done; }
    Status = Root->Open (Root, &File, L"\\EFI\\UniXNU\\Payloads\\READTEST.BIN", EFI_FILE_MODE_READ, 0);
    Root->Close (Root);
    if (Status == EFI_NOT_FOUND) { continue; }
    if (EFI_ERROR (Status)) { goto Done; }
    if (++Matches > 1) {
      File->Close (File); Status = EFI_COMPROMISED_DATA; goto Done;
    }
    Selected = File;
  }
  if (Selected == NULL) { Status = EFI_NOT_FOUND; goto Done; }
  Message (L"unique file found; file-open succeeded");
  Status = ReadAndHash (Selected);
Done:
  if (Selected != NULL) { Selected->Close (Selected); }
  FreePool (Handles);
  return Status;
}

STATIC VOID EFIAPI Ready (EFI_EVENT Event, VOID *Context) {
  EFI_STATUS Status;
  if (Done) { return; }
  Done = TRUE;
  gBS->CloseEvent (Event);
  DEBUG ((DEBUG_ERROR, "[NUWA-FV-FILE] ready enter (read-only diagnostic)\n"));
  Message (L"ReadyToBoot enter; FV driver, no external EFI load");
  Status = AuditVolumes ();
  Print (L"[NUWA-FV-FILE] exit status=%r; hold 30s\r\n", Status);
  DEBUG ((DEBUG_ERROR, "[NUWA-FV-FILE] exit status=%r\n", Status));
  gBS->Stall (30000000);
}

EFI_STATUS EFIAPI NuwaFileAuditDxeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_EVENT Event;
  EFI_STATUS Status;
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Ready, NULL, &gEfiEventReadyToBootGuid, &Event);
  DEBUG ((DEBUG_ERROR, "[NUWA-FV-FILE] registered status=%r\n", Status));
  return Status;
}
