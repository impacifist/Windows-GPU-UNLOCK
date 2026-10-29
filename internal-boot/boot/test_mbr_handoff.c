/* GPL-2.0-only. Synthetic MBR paths and boot-image bytes, no disk/firmware use. */
#define GU_WINDOWS_MBR_SIGNATURE 0x12345678U
#define GU_WINDOWS_MBR_PARTITION 1U
#define GU_WINDOWS_MBR_START 2048ULL
#define GU_WINDOWS_MBR_SIZE 204800ULL
#define GU_WINDOWS_BOOT_BYTES 3U
#define GU_WINDOWS_BOOT_SHA256 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
#include "handoff.c"
STATIC_ASSERT(OFFSET_OF(EFI_FILE_INFO,FileSize)==8,"UEFI FileSize ABI");
STATIC_ASSERT(OFFSET_OF(EFI_FILE_INFO,PhysicalSize)==16,"UEFI PhysicalSize ABI");
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks,mode,loadCalls,closeCalls;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"MBR FAIL line %d\n",__LINE__);exit(1);}}while(0)
typedef struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} DISK_PATH;
static DISK_PATH disk,changed;
static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE table;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL fs;
static EFI_FILE_PROTOCOL root,file;
static const EFI_HANDLE device=(VOID *)1;

EFI_STATUS GuCaptureGraphOnVolume(EFI_SYSTEM_TABLE *t,EFI_FILE_PROTOCOL *f,EFI_FILE_PROTOCOL *r){(void)t;(void)f;(void)r;return EFI_ABORTED;}
static void Init(void) {
  memset(&disk,0,sizeof(disk));
  disk.hd.Header.Type=MEDIA_DEVICE_PATH;disk.hd.Header.SubType=MEDIA_HARDDRIVE_DP;
  disk.hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);
  disk.hd.PartitionNumber=1;disk.hd.PartitionStart=2048;disk.hd.PartitionSize=204800;
  disk.hd.MBRType=1;disk.hd.SignatureType=1;
  disk.hd.Signature[0]=0x78;disk.hd.Signature[1]=0x56;disk.hd.Signature[2]=0x34;disk.hd.Signature[3]=0x12;
  disk.end.Type=END_DEVICE_PATH_TYPE;disk.end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;disk.end.Length[0]=4;
}
static EFI_STATUS EFIAPI Handles(EFI_LOCATE_SEARCH_TYPE t,EFI_GUID *g,VOID *k,UINTN *n,EFI_HANDLE **out) {
  (void)t;(void)g;(void)k;*n=mode==1?2:1;*out=malloc(*n*sizeof(EFI_HANDLE));
  for(UINTN i=0;i<*n;i++)(*out)[i]=device;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Protocol(EFI_HANDLE h,EFI_GUID *g,VOID **out) {
  (void)h;
  if(GuGuidEqual(g,&PathGuid)){*out=mode==9?(VOID *)&changed:&disk;return EFI_SUCCESS;}
  if(GuGuidEqual(g,&FsGuid)){*out=&fs;return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI Alloc(EFI_MEMORY_TYPE t,UINTN n,VOID **out){(void)t;*out=mode==10?NULL:malloc(n);return *out?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;}
static EFI_STATUS EFIAPI Free(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI OpenVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *f,EFI_FILE_PROTOCOL **out){(void)f;*out=&root;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Open(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **out,CHAR16 *path,UINT64 flags,UINT64 attrs) {
  (void)r;(void)attrs;CHECK(flags==EFI_FILE_MODE_READ);
  CHECK(!memcmp(path,L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",sizeof(L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi")));
  *out=&file;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Info(EFI_FILE_PROTOCOL *f,EFI_GUID *g,UINTN *n,VOID *out) {
  (void)f;(void)g;EFI_FILE_INFO *i=out;memset(i,0,sizeof(*i));i->Size=sizeof(*i);i->FileSize=3;
  if(mode==4)i->FileSize=9000000;
  if(mode==5)i->Attribute=EFI_FILE_DIRECTORY;
  *n=mode==6?1:sizeof(*i);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Read(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *out){(void)f;memcpy(out,mode==2?"abd":"abc",3);if(mode==3)(*n)--;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Close(EFI_FILE_PROTOCOL *f){closeCalls++;return (mode==7&&f==&file)||(mode==8&&f==&root)?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI Load(BOOLEAN policy,EFI_HANDLE parent,EFI_DEVICE_PATH_PROTOCOL *path,VOID *bytes,UINTN n,EFI_HANDLE *out) {
  (void)parent;CHECK(!policy && path && n==3 && !memcmp(bytes,"abc",3));loadCalls++;*out=(VOID *)2;return EFI_SUCCESS;
}
int main(void) {
  Init();CHECK(GuWindowsVolumePath(&disk.hd.Header));
  for(int fault=0;fault<11;fault++) {
    changed=disk;
    switch(fault) {
      case 0:changed.hd.Signature[0]^=1;break;
      case 1:changed.hd.PartitionNumber++;break;
      case 2:changed.hd.PartitionStart++;break;
      case 3:changed.hd.PartitionSize++;break;
      case 4:changed.hd.MBRType=2;break;
      case 5:changed.hd.SignatureType=2;break;
      case 6:changed.hd.Signature[4]=1;break;
      case 7:changed.hd.Header.Length[0]=3;break;
      case 8:changed.end.SubType=END_INSTANCE_DEVICE_PATH_SUBTYPE;break;
      case 9:changed.hd.PartitionStart=MAX_UINT64;break;
      case 10:changed.hd.PartitionSize=0;break;
    }
    CHECK(!GuWindowsVolumePath(&changed.hd.Header));
  }
  struct {USB_DEVICE_PATH usb;DISK_PATH path;} usb={0};usb.path=disk;
  usb.usb.Header.Type=MESSAGING_DEVICE_PATH;usb.usb.Header.SubType=MSG_USB_DP;usb.usb.Header.Length[0]=sizeof(USB_DEVICE_PATH);
  CHECK(!GuWindowsVolumePath(&usb.usb.Header));
  usb.usb.Header.SubType=MSG_USB_CLASS_DP;CHECK(!GuWindowsVolumePath(&usb.usb.Header));
  usb.usb.Header.SubType=MSG_USB_WWID_DP;CHECK(!GuWindowsVolumePath(&usb.usb.Header));
  struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL usb,end;} lateUsb={disk.hd,{0},disk.end};
  lateUsb.usb.Type=MESSAGING_DEVICE_PATH;lateUsb.usb.SubType=MSG_USB_CLASS_DP;lateUsb.usb.Length[0]=4;
  CHECK(!GuWindowsVolumePath(&lateUsb.hd.Header));
  struct {HARDDRIVE_DEVICE_PATH first;DISK_PATH path;} duplicate={disk.hd,disk};
  CHECK(!GuWindowsVolumePath(&duplicate.first.Header));
  bs.LocateHandleBuffer=Handles;bs.HandleProtocol=Protocol;bs.FreePool=Free;bs.AllocatePool=Alloc;bs.LoadImage=Load;
  table.BootServices=&bs;Table=&table;fs.OpenVolume=OpenVolume;
  root.Open=Open;root.Close=Close;file.GetInfo=Info;file.Read=Read;file.Close=Close;
  EFI_HANDLE found=NULL;mode=0;
  CHECK(FindPartition(&GuWindowsPartition,&found,TRUE)==EFI_SUCCESS && found==device);
  mode=1;CHECK(FindPartition(&GuWindowsPartition,&found,TRUE)==EFI_COMPROMISED_DATA);
  changed=disk;changed.hd.PartitionSize++;mode=9;
  CHECK(FindPartition(&GuWindowsPartition,&found,TRUE)==EFI_NOT_FOUND);
  for(mode=0;mode<=10;mode++) {
    loadCalls=closeCalls=0;
    EFI_STATUS s=LoadMbrWindows(device,&disk.hd.Header,3,GU_WINDOWS_BOOT_SHA256);
    CHECK((mode<2?s==EFI_SUCCESS:s!=EFI_SUCCESS) && loadCalls==(mode<2));
    CHECK(closeCalls==2);
  }
  printf("PASS MBR volume/image: %d checks; synthetic paths and bytes only\n",checks);
  return 0;
}
