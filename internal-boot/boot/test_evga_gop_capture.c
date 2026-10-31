/* Actual diagnostic Details path, no GPU access or driver execution. */
#define GU_EVGA_STOP_ONLY
#include "diagnostic.c"
static EFI_GRAPHICS_OUTPUT_PROTOCOL G;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE M;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
static BOOLEAN HasPath;
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *guid,VOID **out){
  (void)h;*out=NULL;
  if(guid==&GopGuid){*out=&G;return EFI_SUCCESS;}
  if(guid==&PathGuid && HasPath){*out=&G;return EFI_SUCCESS;}
  return EFI_UNSUPPORTED;
}
static BOOLEAN Capture(VOID){
  EFI_BOOT_SERVICES bs={0};bs.HandleProtocol=Handle;
  EFI_SYSTEM_TABLE table={0};table.BootServices=&bs;
  static CAPTURE c;c=(CAPTURE){0};c.Table=&table;c.Complete=TRUE;c.Io=EFI_SUCCESS;
  Details(&c,(EFI_HANDLE)1,&GopGuid);return c.Complete;
}
static VOID Init(VOID){G=(EFI_GRAPHICS_OUTPUT_PROTOCOL){0};M=(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE){0};G.Mode=&M;HasPath=FALSE;}
int main(VOID){
  Init();if(!Capture())return 1; /* Captured pathless, framebufferless aggregate. */
  Init();HasPath=TRUE;if(Capture())return 2;
  Init();M.SizeOfInfo=1;if(Capture())return 3;
  Init();M.Info=&Info;M.SizeOfInfo=sizeof(Info)-1;if(Capture())return 4;
  Init();M.FrameBufferBase=1;if(Capture())return 5;
  Init();M.FrameBufferSize=1;if(Capture())return 6;
  Init();G.Mode=NULL;if(Capture())return 7;
  Init();M.Info=&Info;M.SizeOfInfo=sizeof(Info);HasPath=TRUE;if(!Capture())return 8;
  Init();M.Info=&Info;if(!Capture())return 9; /* Size zero: pointer is not dereferenced. */
  return 0;
}
