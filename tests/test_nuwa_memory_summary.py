"""Execute the exact firmware memory-summary code with unordered and invalid maps."""
import pathlib,subprocess,tempfile,unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
HEADER=ROOT/'Platforms/Xiaomi/nuwaPkg/Drivers/NuwaBootAuditDxe'
HARNESS=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef uint8_t UINT8; typedef uint32_t UINT32; typedef uint64_t UINT64; typedef size_t UINTN; typedef void VOID; typedef int EFI_STATUS;
#define STATIC static
#define CONST const
#define MAX_UINT64 UINT64_MAX
#define EFI_SUCCESS 0
#define EFI_INVALID_PARAMETER 2
#define EFI_BUFFER_TOO_SMALL 5
#define EFI_COMPROMISED_DATA 33
#define EfiConventionalMemory 7
#define EFI_MEMORY_WB 8
#define ZeroMem(p,n) memset(p,0,n)
typedef struct {UINT32 Type,Pad;UINT64 PhysicalStart,VirtualStart,NumberOfPages,Attribute;} EFI_MEMORY_DESCRIPTOR;
#include "NuwaMemorySummary.h"
static NUWA_AUDIT_RANGE ranges[NUWA_AUDIT_MAX_RANGES];
static NUWA_AUDIT_SUMMARY sum;
static EFI_MEMORY_DESCRIPTOR map[129];
static void row(unsigned i,UINT64 start,UINT64 pages,UINT64 attr) {map[i]=(EFI_MEMORY_DESCRIPTOR){7,0,start,0,pages,attr};}
static EFI_STATUS run(UINTN count) {return NuwaSummarizeMemory(map,count*sizeof(map[0]),sizeof(map[0]),ranges,&sum);}
int main(void) {
 row(0,0x3000,2,8);row(1,0x1000,2,8);row(2,0x7000,2,1);
 assert(run(3)==0&&sum.MergedCount==2&&sum.TotalBytes==6*4096&&sum.LargestBase==0x1000&&sum.LargestBytes==4*4096&&sum.LargestWbBytes==4*4096);
 row(0,0x1000,1,8);row(1,0x2000,1,1);assert(run(2)==0&&sum.MergedCount==2);
 row(0,0x1000,1,8);row(1,0x3000,1,8);assert(run(2)==0&&sum.MergedCount==2);
 row(0,0x1000,2,8);row(1,0x2000,1,8);assert(run(2)==EFI_COMPROMISED_DATA);
 row(0,0x1000,1,8);row(1,0x1000,1,1);assert(run(2)==EFI_COMPROMISED_DATA);
 row(0,0x1001,1,8);assert(run(1)==EFI_COMPROMISED_DATA);
 row(0,0x1000,0,8);assert(run(1)==EFI_COMPROMISED_DATA);
 row(0,0,UINT64_MAX,8);assert(run(1)==EFI_COMPROMISED_DATA);
 row(0,UINT64_MAX-4095,1,8);assert(run(1)==EFI_COMPROMISED_DATA);
 row(0,0x1000,1,8);map[0].Type=0;assert(run(1)==0&&sum.MergedCount==0&&sum.TotalBytes==0&&sum.LargestBytes==0);
 for(unsigned i=0;i<129;i++)row(i,0x1000+i*0x2000,1,8);
 assert(run(129)==EFI_BUFFER_TOO_SMALL);
 assert(NuwaSummarizeMemory(map,sizeof(map[0])-1,sizeof(map[0]),ranges,&sum)==EFI_INVALID_PARAMETER);
 assert(NuwaSummarizeMemory(map,sizeof(map[0]),32,ranges,&sum)==EFI_INVALID_PARAMETER);
 // Actual UEFI stride can exceed sizeof(EFI_MEMORY_DESCRIPTOR).
 unsigned char extended[2*48]={0};row(0,0x1000,1,8);row(1,0x2000,1,8);
 memcpy(extended,&map[0],40);memcpy(extended+48,&map[1],40);
 assert(NuwaSummarizeMemory(extended,sizeof(extended),48,ranges,&sum)==0&&sum.LargestBytes==8192);
 return 0;
}
'''
class MemorySummaryTests(unittest.TestCase):
 def test_actual_summary_boundaries_order_and_attributes(self):
  with tempfile.TemporaryDirectory() as folder:
   p=pathlib.Path(folder);(p/'test.c').write_text(HARNESS)
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(HEADER),str(p/'test.c'),'-o',str(p/'memory-test')],check=True)
   subprocess.run([str(p/'memory-test')],check=True)
if __name__=='__main__':unittest.main()
