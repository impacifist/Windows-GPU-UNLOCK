/* SPDX-License-Identifier: GPL-2.0-only */
/* Host-only bridge for comparing the production C decoder with Python. */
#include "../gpu-path.h"
#include <stdio.h>
#include <string.h>
static int Digit(char c){return c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;}
int main(void) {
  unsigned bound;char hex[513];int fields;
  while((fields=scanf("%u %512s",&bound,hex))==2) {
    UINT8 data[256]={0};UINTN length=strlen(hex),controller=999,total=999;
    if(bound>sizeof(data) || length>512 || length%2)return 2;
    for(UINTN i=0;i<length/2;i++) {
      int a=Digit(hex[i*2]),b=Digit(hex[i*2+1]);if(a<0 || b<0)return 2;
      data[i]=(UINT8)(a*16+b);
    }
    BOOLEAN valid=GuDecodeGpuDisplayPath(data,bound,&controller,&total);
    printf("%u %llu %llu\n",(unsigned)valid,(unsigned long long)controller,(unsigned long long)total);
  }
  return fields==EOF?0:2;
}
