"""Compile the actual publisher with host service doubles; no hardware claims."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DRIVER = ROOT / 'Platforms/Xiaomi/nuwaPkg/Drivers/NuwaBootMemoryDxe'

STUB = r'''
#ifndef HOST_UEFI_H
#define HOST_UEFI_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef uint8_t UINT8; typedef uint32_t UINT32; typedef int32_t INT32;
typedef uint64_t UINT64; typedef size_t UINTN; typedef int BOOLEAN;
typedef void VOID; typedef void *EFI_HANDLE; typedef void EFI_SYSTEM_TABLE;
typedef UINT64 EFI_STATUS; typedef UINT64 EFI_LBA;
#define STATIC static
#define CONST const
#define IN
#define OUT
#define EFIAPI
#define TRUE 1
#define FALSE 0
#define MAX_UINTN SIZE_MAX
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
#define EFI_BAD_BUFFER_SIZE 4
#define EFI_WRITE_PROTECTED 8
#define EFI_NOT_FOUND 14
#define EFI_ACCESS_DENIED 15
#define EFI_MEDIA_CHANGED 13
#define EFI_COMPROMISED_DATA 33
#define EFI_ERROR(x) ((x)!=0)
#define DEBUG_ERROR 1
#define DEBUG(x) ((void)0)
#define AddMem 1
#define EFI_RESOURCE_SYSTEM_MEMORY 0
#define EfiReservedMemoryType 0
#define ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK 4
#define EFI_BLOCK_IO_PROTOCOL_REVISION 1
#define HARDWARE_DEVICE_PATH 1
#define HW_VENDOR_DP 4
#define END_DEVICE_PATH_TYPE 127
#define END_ENTIRE_DEVICE_PATH_SUBTYPE 255
typedef struct {UINT32 a; uint16_t b,c; UINT8 d[8];} EFI_GUID;
typedef struct {UINT8 Type,SubType,Length[2];} EFI_DEVICE_PATH_PROTOCOL;
typedef struct {EFI_DEVICE_PATH_PROTOCOL Header; EFI_GUID Guid;} VENDOR_DEVICE_PATH;
typedef struct {UINT32 MediaId; BOOLEAN RemovableMedia,MediaPresent,LogicalPartition,ReadOnly,WriteCaching; UINT32 BlockSize,IoAlign; EFI_LBA LastBlock;} EFI_BLOCK_IO_MEDIA;
typedef struct _BIO EFI_BLOCK_IO_PROTOCOL;
struct _BIO {UINT64 Revision; EFI_BLOCK_IO_MEDIA *Media; EFI_STATUS (*Reset)(EFI_BLOCK_IO_PROTOCOL*,BOOLEAN); EFI_STATUS (*Read)(EFI_BLOCK_IO_PROTOCOL*,UINT32,EFI_LBA,UINTN,VOID*); EFI_STATUS (*Write)(EFI_BLOCK_IO_PROTOCOL*,UINT32,EFI_LBA,UINTN,VOID*); EFI_STATUS (*Flush)(EFI_BLOCK_IO_PROTOCOL*);};
typedef struct {char *Name; UINT64 Address,Length; UINT32 HobOption,ResourceType,MemoryType,ArmAttributes;} EFI_MEMORY_REGION_DESCRIPTOR;
typedef struct {UINTN size; UINT64 dtb;} EFI_HOB_GUID_TYPE;
#define GET_GUID_HOB_DATA_SIZE(h) ((h)->size)
#define GET_GUID_HOB_DATA(h) (&(h)->dtb)
static EFI_GUID gSiliciumEntryArgHobGuid,gEfiDevicePathProtocolGuid,gEfiBlockIoProtocolGuid;
static UINT64 ReadUnaligned64(CONST VOID *p) {UINT64 n; memcpy(&n,p,8);return n;}
static UINT32 ReadUnaligned32(CONST VOID *p) {UINT32 n; memcpy(&n,p,4);return n;}
#define Fdt64ToCpu(x) __builtin_bswap64(x)
#define Fdt32ToCpu(x) __builtin_bswap32(x)
#define AsciiStrCmp strcmp
#define CopyMem memcpy
static UINT8 arena[4096];
static EFI_MEMORY_REGION_DESCRIPTOR map;
static EFI_HOB_GUID_TYPE hob;
static int no_hob,header_bad,chosen_bad,no_prop,prop_len,header_reads,installs,crc_calls,stall_calls;
static UINT32 fdt_size; static UINT64 start_be,end_be;
static EFI_STATUS install_status;
static void GetMemoryMap(EFI_MEMORY_REGION_DESCRIPTOR **m,UINT8 *n) {*m=&map;*n=1;}
static EFI_HOB_GUID_TYPE *GetFirstGuidHob(EFI_GUID *g) {(void)g;return no_hob?NULL:&hob;}
static int FdtCheckHeader(VOID *p) {(void)p;header_reads++;return header_bad;}
static UINT32 FdtTotalSize(VOID *p) {(void)p;return fdt_size;}
static INT32 FdtPathOffset(CONST VOID *p,CONST char *s) {(void)p;(void)s;return chosen_bad?-1:0;}
static CONST VOID *FdtGetProp(CONST VOID *p,INT32 n,CONST char *s,INT32 *len) {(void)p;(void)n;*len=prop_len;return no_prop?NULL:(strstr(s,"start")?&start_be:&end_be);}
static EFI_STATUS host_crc(VOID *p,UINTN n,UINT32 *v) {(void)p;(void)n;crc_calls++;*v=0;return 0;}
static EFI_STATUS host_install(EFI_HANDLE *h,...) {(void)h;installs++;return install_status;}
static EFI_STATUS host_stall(UINTN us) {assert(us==5000000);stall_calls++;return 0;}
static struct {EFI_STATUS (*CalculateCrc32)(VOID*,UINTN,UINT32*);EFI_STATUS (*InstallMultipleProtocolInterfaces)(EFI_HANDLE*,...);EFI_STATUS (*Stall)(UINTN);} services={host_crc,host_install,host_stall},*gBS=&services;
#endif
'''

HARNESS = r'''
#include "NuwaBootMemoryDxe.c"
static void reset(void) {
 memset(arena,0,sizeof(arena));
 map=(EFI_MEMORY_REGION_DESCRIPTOR){"Kernel",(UINTN)arena,sizeof(arena),AddMem,EFI_RESOURCE_SYSTEM_MEMORY,EfiReservedMemoryType,ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK};
 hob=(EFI_HOB_GUID_TYPE){8,(UINTN)arena};
 no_hob=header_bad=chosen_bad=no_prop=header_reads=installs=crc_calls=stall_calls=0;
 prop_len=8;fdt_size=40;install_status=EFI_SUCCESS;
 start_be=__builtin_bswap64((UINTN)arena+128);end_be=__builtin_bswap64((UINTN)arena+1024);
}
static void expect(EFI_STATUS status) {assert(NuwaBootMemoryDxeEntry(NULL,NULL)==status);assert(stall_calls==1);}
int main(void) {
 reset();no_hob=1;expect(EFI_NOT_FOUND);assert(!header_reads&&!installs);
 reset();hob.size=7;expect(EFI_NOT_FOUND);assert(!header_reads);
 reset();hob.dtb=0;expect(EFI_INVALID_PARAMETER);assert(!header_reads);
 reset();hob.dtb++;expect(EFI_INVALID_PARAMETER);assert(!header_reads);
 reset();hob.dtb=4;expect(EFI_ACCESS_DENIED);assert(!header_reads);
 reset();map.MemoryType=7;expect(EFI_ACCESS_DENIED);assert(!header_reads);
 reset();header_bad=1;expect(EFI_COMPROMISED_DATA);assert(header_reads==1);
 reset();fdt_size=39;expect(EFI_COMPROMISED_DATA);
 reset();fdt_size=sizeof(arena)+1;expect(EFI_ACCESS_DENIED);assert(!installs);
 reset();chosen_bad=1;expect(EFI_NOT_FOUND);
 reset();no_prop=1;expect(EFI_INVALID_PARAMETER);
 reset();prop_len=3;expect(EFI_INVALID_PARAMETER);
 reset();end_be=start_be;expect(EFI_ACCESS_DENIED);assert(!crc_calls&&!installs);
 reset();end_be=__builtin_bswap64((UINTN)arena+sizeof(arena)+1);expect(EFI_ACCESS_DENIED);assert(!crc_calls&&!installs);
 reset();expect(EFI_NOT_FOUND);assert(!installs);
 reset();memcpy(arena+128,mNuwaVolumeMagic,16);expect(EFI_COMPROMISED_DATA);assert(!installs);
 reset();memcpy(arena+128,mNuwaVolumeMagic,16);arena[144]=1;arena[148]=64;arena[153]=2;
 expect(EFI_SUCCESS);assert(installs==1&&crc_calls==1);assert(mRamBase==arena+192&&mMedia.LastBlock==0&&mMedia.ReadOnly);
 UINT8 buffer[512];assert(RamBlockRead(&mBlockIo,1,0,512,buffer)==EFI_SUCCESS);
 assert(RamBlockRead(&mBlockIo,1,1,512,buffer)==EFI_INVALID_PARAMETER);
 assert(RamBlockWrite(&mBlockIo,1,0,512,buffer)==EFI_WRITE_PROTECTED);
 reset();memcpy(arena+128,mNuwaVolumeMagic,16);arena[144]=1;arena[148]=64;arena[153]=2;install_status=EFI_ACCESS_DENIED;
 expect(EFI_ACCESS_DENIED);assert(installs==1);
 return 0;
}
'''

class PublisherTests(unittest.TestCase):
    def test_validation_status_and_mapping_before_read(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder)
            for name in ('Uefi.h','Library/BaseLib.h','Library/BaseMemoryLib.h','Library/DebugLib.h','Library/FdtLib.h','Library/HobLib.h','Library/MemoryMapLib.h','Library/UefiBootServicesTableLib.h','Protocol/BlockIo.h','Protocol/DevicePath.h'):
                header = path/name
                header.parent.mkdir(exist_ok=True)
                header.write_text('#include "HostUefi.h"\n')
            (path/'HostUefi.h').write_text(STUB)
            (path/'test.c').write_text(HARNESS)
            binary=path/'publisher-test'
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fsanitize=undefined','-DNUWA_VOLUME_DIAGNOSTIC_PAUSE_US=5000000','-I',str(path),'-I',str(DRIVER),str(path/'test.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)

if __name__ == '__main__':
    unittest.main()
