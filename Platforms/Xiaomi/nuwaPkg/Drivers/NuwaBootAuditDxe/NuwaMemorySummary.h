// SPDX-License-Identifier: BSD-2-Clause
#ifndef NUWA_MEMORY_SUMMARY_H
#define NUWA_MEMORY_SUMMARY_H
#define NUWA_AUDIT_MAX_RANGES 128

typedef struct {
  UINT64 Start;
  UINT64 End;
  UINT64 Attributes;
} NUWA_AUDIT_RANGE;

typedef struct {
  UINTN DescriptorCount;
  UINTN ConventionalCount;
  UINTN MergedCount;
  UINT64 TotalBytes;
  UINT64 LargestBase;
  UINT64 LargestBytes;
  UINT64 LargestWbBase;
  UINT64 LargestWbBytes;
} NUWA_AUDIT_SUMMARY;

STATIC EFI_STATUS
NuwaSummarizeMemory (
  CONST VOID *Map, UINTN MapBytes, UINTN DescriptorBytes,
  NUWA_AUDIT_RANGE *Ranges, NUWA_AUDIT_SUMMARY *Summary)
{
  UINTN Index;
  UINTN Other;
  UINTN Out;
  UINTN Count;
  UINT64 Bytes;
  CONST EFI_MEMORY_DESCRIPTOR *Entry;
  NUWA_AUDIT_RANGE Value;

  if ((Map == NULL) || (Ranges == NULL) || (Summary == NULL) ||
      (DescriptorBytes < sizeof (EFI_MEMORY_DESCRIPTOR)) || ((DescriptorBytes & 7) != 0) ||
      (MapBytes == 0) || ((MapBytes % DescriptorBytes) != 0) ||
      (MapBytes / DescriptorBytes > 2048)) {
    return EFI_INVALID_PARAMETER;
  }
  ZeroMem (Summary, sizeof (*Summary));
  Summary->DescriptorCount = MapBytes / DescriptorBytes;
  Count = 0;
  for (Index = 0; Index < Summary->DescriptorCount; Index++) {
    Entry = (CONST EFI_MEMORY_DESCRIPTOR *)((CONST UINT8 *)Map + Index * DescriptorBytes);
    if (Entry->Type != EfiConventionalMemory) {
      continue;
    }
    if ((Entry->NumberOfPages == 0) || ((Entry->PhysicalStart & 4095) != 0) ||
        (Entry->NumberOfPages > MAX_UINT64 / 4096)) {
      return EFI_COMPROMISED_DATA;
    }
    Bytes = Entry->NumberOfPages * 4096;
    if ((Entry->PhysicalStart > MAX_UINT64 - Bytes) ||
        (Summary->TotalBytes > MAX_UINT64 - Bytes)) {
      return EFI_COMPROMISED_DATA;
    }
    if (Count == NUWA_AUDIT_MAX_RANGES) {
      return EFI_BUFFER_TOO_SMALL; // Never report a partial list as complete.
    }
    Ranges[Count].Start = Entry->PhysicalStart;
    Ranges[Count].End = Entry->PhysicalStart + Bytes;
    Ranges[Count].Attributes = Entry->Attribute;
    Summary->TotalBytes += Bytes;
    Count++;
  }
  Summary->ConventionalCount = Count;
  // UEFI descriptor order is not assumed. Keep attributes when coalescing.
  for (Index = 1; Index < Count; Index++) {
    Value = Ranges[Index];
    Other = Index;
    while ((Other > 0) && (Ranges[Other - 1].Start > Value.Start)) {
      Ranges[Other] = Ranges[Other - 1];
      Other--;
    }
    Ranges[Other] = Value;
  }
  Out = 0;
  for (Index = 0; Index < Count; Index++) {
    Value = Ranges[Index];
    if ((Out > 0) && (Value.Start < Ranges[Out - 1].End)) {
      return EFI_COMPROMISED_DATA;
    }
    if ((Out > 0) && (Value.Start == Ranges[Out - 1].End) &&
        (Value.Attributes == Ranges[Out - 1].Attributes)) {
      Ranges[Out - 1].End = Value.End;
    } else {
      Ranges[Out++] = Value;
    }
  }
  Summary->MergedCount = Out;
  for (Index = 0; Index < Out; Index++) {
    Bytes = Ranges[Index].End - Ranges[Index].Start;
    if (Bytes > Summary->LargestBytes) {
      Summary->LargestBytes = Bytes;
      Summary->LargestBase = Ranges[Index].Start;
    }
    if (((Ranges[Index].Attributes & EFI_MEMORY_WB) != 0) &&
        (Bytes > Summary->LargestWbBytes)) {
      Summary->LargestWbBytes = Bytes;
      Summary->LargestWbBase = Ranges[Index].Start;
    }
  }
  return EFI_SUCCESS;
}
#endif
