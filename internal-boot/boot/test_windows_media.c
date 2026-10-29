/* GPL-2.0-only. Default GPT Windows matching regression; no hardware access. */
#include "media.h"
#include <stdio.h>
#include <string.h>
int main(void) {
  struct {HARDDRIVE_DEVICE_PATH hd;EFI_DEVICE_PATH_PROTOCOL end;} p={0};
  p.hd.Header.Type=MEDIA_DEVICE_PATH;p.hd.Header.SubType=MEDIA_HARDDRIVE_DP;
  p.hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);p.hd.MBRType=p.hd.SignatureType=2;
  p.hd.PartitionSize=100;memcpy(p.hd.Signature,&GuWindowsPartition,16);
  p.end.Type=END_DEVICE_PATH_TYPE;p.end.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;p.end.Length[0]=4;
  if(!GuWindowsVolumePath(&p.hd.Header))return 1;
  p.hd.Signature[0]^=1;if(GuWindowsVolumePath(&p.hd.Header))return 2;p.hd.Signature[0]^=1;
  p.hd.SignatureType=1;if(GuWindowsVolumePath(&p.hd.Header))return 3;
  p.hd.SignatureType=2;p.hd.MBRType=1;if(GuWindowsVolumePath(&p.hd.Header))return 4;
  puts("PASS default GPT Windows volume: 4 checks");return 0;
}
