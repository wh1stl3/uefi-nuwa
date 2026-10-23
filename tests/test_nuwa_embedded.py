"""Compile the actual FV probe driver with host services; no hardware claims."""
import pathlib
import subprocess
import tempfile
import unittest
from test_nuwa_publisher import STUB
ROOT=pathlib.Path(__file__).resolve().parents[1]
DRIVER=ROOT/'Platforms/Xiaomi/nuwaPkg/Drivers/NuwaEmbeddedVolumeDxe'
EXTRA=r'''
#define EFI_SECTION_RAW 0x19
static UINT8 fixture[4*1024*1024];
static UINTN resource_size; static EFI_STATUS resource_status; static int frees,resource_null;
static EFI_STATUS GetSectionFromAnyFv(EFI_GUID *g,UINTN type,UINTN instance,VOID **data,UINTN *size) {
 (void)g;assert(type==0x19&&instance==0);*data=resource_null?NULL:fixture;*size=resource_size;return resource_status;
}
static void FreePool(VOID *p) {assert(p==fixture);frees++;}
'''
HARNESS=r'''
#include "NuwaEmbeddedVolumeDxe.c"
static UINT32 returned_crc;
static EFI_STATUS fixture_crc(VOID *p,UINTN n,UINT32 *v) {assert(p==fixture&&n==sizeof(fixture));*v=returned_crc;return 0;}
static void reset(void) {
 memset(fixture,0,sizeof(fixture));fixture[510]=0x55;fixture[511]=0xaa;
 resource_size=sizeof(fixture);resource_status=0;resource_null=0;frees=installs=stall_calls=0;install_status=0;
 returned_crc=0x54fe5070;gBS->CalculateCrc32=fixture_crc;mRamBase=NULL;
}
int main(void) {
 reset();resource_status=EFI_NOT_FOUND;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_NOT_FOUND);assert(!frees&&!installs);
 reset();resource_size--;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_COMPROMISED_DATA);assert(frees==1&&!installs);
 reset();resource_null=1;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_COMPROMISED_DATA);assert(!frees&&!installs);
 reset();fixture[511]=0;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_COMPROMISED_DATA);assert(frees==1&&!installs);
 reset();returned_crc++;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_COMPROMISED_DATA);assert(frees==1&&!installs);
 reset();assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==0);assert(!frees&&installs==1&&stall_calls==1&&mRamBase==fixture);
 assert(mMedia.ReadOnly&&mMedia.LastBlock==8191);
 UINT8 buffer[512];assert(RamBlockRead(&mBlockIo,1,8191,512,buffer)==0);assert(RamBlockRead(&mBlockIo,1,8191,1024,buffer)==EFI_INVALID_PARAMETER);
 reset();install_status=EFI_ACCESS_DENIED;assert(NuwaEmbeddedVolumeDxeEntry(NULL,NULL)==EFI_ACCESS_DENIED);assert(frees==1&&mRamBase==NULL);
 return 0;
}
'''
class EmbeddedTests(unittest.TestCase):
 def test_resource_validation_ownership_and_install_errors(self):
  with tempfile.TemporaryDirectory() as folder:
   p=pathlib.Path(folder)
   for name in ('Uefi.h','Library/BaseMemoryLib.h','Library/DebugLib.h','Library/DxeServicesLib.h','Library/MemoryAllocationLib.h','Library/UefiBootServicesTableLib.h','Protocol/BlockIo.h','Protocol/DevicePath.h'):
    h=p/name;h.parent.mkdir(exist_ok=True);h.write_text('#include "HostUefi.h"\n')
   (p/'HostUefi.h').write_text(STUB+"\n#ifndef HOST_FV_H\n#define HOST_FV_H\n"+EXTRA+"\n#endif\n")
   (p/'test.c').write_text(HARNESS)
   binary=p/'embedded-test'
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-unused-variable','-fsanitize=undefined','-I',str(p),'-I',str(DRIVER),str(p/'test.c'),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
