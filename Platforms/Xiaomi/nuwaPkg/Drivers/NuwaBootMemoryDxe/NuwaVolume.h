// SPDX-License-Identifier: BSD-2-Clause
// v1 transport: 64-byte header followed by a complete 512-byte-sector volume.
// Integer fields are little endian; no structure casts or alignment assumptions.
#ifndef NUWA_VOLUME_H
#define NUWA_VOLUME_H

#define NUWA_VOLUME_HEADER_SIZE 64

typedef BOOLEAN (*NUWA_VOLUME_CRC)(CONST UINT8 *Data, UINTN Size, UINT32 *Result);

typedef enum {
  NuwaVolumeAbsent,
  NuwaVolumeFound,
  NuwaVolumeInvalid,
  NuwaVolumeDuplicate
} NUWA_VOLUME_RESULT;

STATIC CONST UINT8 mNuwaVolumeMagic[16] = {
  0x4e, 0x55, 0x57, 0x41, 0x2d, 0x56, 0x4f, 0x4c,
  0xa3, 0x71, 0x9c, 0x4e, 0x82, 0xd6, 0x50, 0xb9
};

STATIC UINT64
NuwaReadLe (CONST UINT8 *Data, UINTN Count)
{
  UINT64 Value;
  UINTN Index;
  Value = 0;
  for (Index = 0; Index < Count; Index++) {
    Value |= (UINT64)Data[Index] << (8 * Index);
  }
  return Value;
}

// Caller must validate mapping and lifetime for the entire supplied extent.
// CRC is transport corruption detection, not a signature/authentication claim.
STATIC NUWA_VOLUME_RESULT
NuwaFindVolume (
  CONST UINT8 *Data, UINTN Size, NUWA_VOLUME_CRC Crc,
  UINTN *PayloadOffset, UINTN *PayloadSize
  )
{
  UINTN Pos;
  UINTN Index;
  UINT64 Length;
  UINT32 ActualCrc;
  BOOLEAN Found;

  *PayloadOffset = 0;
  *PayloadSize = 0;
  Found = FALSE;
  Pos = 0;
  while ((Pos <= Size) && (Size - Pos >= sizeof (mNuwaVolumeMagic))) {
    for (Index = 0; Index < sizeof (mNuwaVolumeMagic); Index++) {
      if (Data[Pos + Index] != mNuwaVolumeMagic[Index]) {
        break;
      }
    }
    if (Index != sizeof (mNuwaVolumeMagic)) {
      Pos++;
      continue;
    }
    if (Found) {
      return NuwaVolumeDuplicate;
    }
    if ((Size - Pos < NUWA_VOLUME_HEADER_SIZE) ||
        (NuwaReadLe (Data + Pos + 16, 4) != 1) ||
        (NuwaReadLe (Data + Pos + 20, 4) != NUWA_VOLUME_HEADER_SIZE)) {
      return NuwaVolumeInvalid;
    }
    for (Index = 36; Index < NUWA_VOLUME_HEADER_SIZE; Index++) {
      if (Data[Pos + Index] != 0) {
        return NuwaVolumeInvalid;
      }
    }
    Length = NuwaReadLe (Data + Pos + 24, 8);
    if ((Length == 0) || ((Length % 512) != 0) ||
        (Length > Size - Pos - NUWA_VOLUME_HEADER_SIZE)) {
      return NuwaVolumeInvalid;
    }
    if (!Crc (Data + Pos + NUWA_VOLUME_HEADER_SIZE, (UINTN)Length, &ActualCrc) ||
        (ActualCrc != (UINT32)NuwaReadLe (Data + Pos + 32, 4))) {
      return NuwaVolumeInvalid;
    }
    *PayloadOffset = Pos + NUWA_VOLUME_HEADER_SIZE;
    *PayloadSize = (UINTN)Length;
    Found = TRUE;
    // Volume contents may contain magic bytes as ordinary file data.
    Pos = *PayloadOffset + *PayloadSize;
  }
  return Found ? NuwaVolumeFound : NuwaVolumeAbsent;
}
#endif
