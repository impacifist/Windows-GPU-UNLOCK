/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ #include "../checks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned Checks, Failures;

static void Check(int Pass, const char *Name) {
  ++Checks;
  if (!Pass) {
    ++Failures;
    fprintf(stderr, "FAIL %s\n", Name);
  }
}

static void Byte(UINT32 C[64], unsigned Offset, unsigned Value) {
  unsigned Shift = (Offset & 3U) * 8U;
  C[Offset / 4U] = (C[Offset / 4U] & ~(0xffU << Shift)) |
                  ((Value & 0xffU) << Shift);
}

static void Word(UINT32 C[64], unsigned Offset, unsigned Value) {
  Byte(C, Offset, Value);
  Byte(C, Offset + 1U, Value >> 8U);
}

static void Basic(UINT32 C[64]) {
  memset(C, 0, 256);
  C[0] = GU_BOARD_ID;
  C[1] = 0x00100002;
  C[2] = 0x030000a1;
  C[3] = 0x00800000;
  C[0x2c / 4] = GU_BOARD_SUBSYSTEM;
  C[0x34 / 4] = 0x40;
  C[0x40 / 4] = 0x00034801;
  C[0x48 / 4] = 0x00020010;
  C[0x4c / 4] = 0x10000000;
}

static int Valid(const UINT32 C[64]) {
  GU_CAPS Caps;
  return GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps);
}

static void Reject(const UINT32 C[64], const char *Name) {
  GU_CAPS Caps = {0xffff, 0xffff, 0xffff, 0xffff, 0xff};
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps), Name);
  Check(Caps.pm == 0 && Caps.pcie == 0 && Caps.msi == 0 && Caps.msix == 0 &&
        Caps.pcie_version == 0, "rejection clears capability output");
}

static int LoadCapture(const char *Path, UINT32 C[64]) {
  FILE *File = fopen(Path, "rb");
  char Line[256];
  unsigned Count = 0;
  unsigned long long Offset, Value;
  if (File == NULL) {
    fprintf(stderr, "cannot open fixture %s\n", Path);
    return 0;
  }
  while (fgets(Line, sizeof(Line), File) != NULL) {
    if (Count == 64) break; /* First DISPLAY PCI in RUN00 is the target. */
    if (sscanf(Line, "config_offset=0x%llx", &Offset) != 1) continue;
    if (Count >= 64 || Offset != Count * 4U ||
        fgets(Line, sizeof(Line), File) == NULL ||
        sscanf(Line, "value=0x%llx", &Value) != 1 || Value > 0xffffffffULL) {
      fclose(File);
      fprintf(stderr, "malformed config fixture\n");
      return 0;
    }
    C[Count++] = (UINT32)Value;
  }
  fclose(File);
  if (Count != 64) fprintf(stderr, "fixture has %u config dwords, expected 64\n", Count);
  return Count == 64;
}

/* Historical RUN00 linked-capability regression, independent of input machine. */
static void TestLegacyCapture(const UINT32 Captured[64]) {
  UINT32 C[64], Before[64];
  GU_CAPS Caps;
  memcpy(C, Captured, sizeof(C));
  memcpy(Before, C, sizeof(C));
  Check(GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps), "legacy RUN00 capability layout accepted");
  Check(Caps.pm == 0x60 && Caps.msi == 0x68 && Caps.pcie == 0x78 &&
        Caps.msix == 0 && Caps.pcie_version == 2,
        "capture returns linked caps only; c8 is unlinked");
  Check(memcmp(C, Before, sizeof(C)) == 0, "validation does not modify config");
  C[0]=0x220610de; Reject(C,"RTX3080 rejected with otherwise real config");
  memcpy(C,Before,sizeof(C));C[0]=0x164e1002;C[11]=0x88771043;
  Reject(C,"AMD display cannot be target");memcpy(C,Before,sizeof(C));
  Byte(C, 0xb5, 0xc8);
  Check(GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps) && Caps.msix == 0xc8,
        "synthetically linked captured MSI-X is disabled and accepted");
  Word(C, 0xca, 0x8005);
  Reject(C, "linked enabled MSI-X rejected");
}

static unsigned ConfigByte(const UINT32 C[64], unsigned Offset) {
  return (C[Offset / 4] >> ((Offset & 3U) * 8U)) & 0xffU;
}

static int Linked(const UINT32 C[64], unsigned Offset, unsigned Id) {
  unsigned At=ConfigByte(C,0x34);
  for(unsigned Step=0;At && Step<48;Step++) {
    if(At<0x40 || At>0xfc || (At&3))return 0;
    if(At==Offset)return ConfigByte(C,At)==Id;
    At=ConfigByte(C,At+1);
  }
  return 0;
}

static void TestCapture(const UINT32 Captured[64]) {
  UINT32 C[64],Before[64];GU_CAPS Caps;
  memcpy(C,Captured,sizeof(C));memcpy(Before,C,sizeof(C));
  int Accepted=GuCheckConfig(C,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,0,&Caps);
  Check(Accepted,"captured config satisfies this exact profile");
  Check(memcmp(C,Before,sizeof(C))==0,"actual capture validation preserves all config bytes");
  if(!Accepted)return;
  Check(Caps.pm && Linked(C,Caps.pm,1),"returned PM capability is linked in this capture");
  Check(Caps.pcie && Linked(C,Caps.pcie,0x10),"returned PCIe capability is linked in this capture");
  Check(Caps.pcie_version==(ConfigByte(C,Caps.pcie+2)&15),"returned PCIe version comes from the linked capability");
  Check(!Caps.msi || Linked(C,Caps.msi,5),"optional MSI is linked if present");
  Check(!Caps.msix || Linked(C,Caps.msix,0x11),"optional MSI-X is linked if present");
  C[0]^=1;Reject(C,"different captured PCI identity rejected");
  memcpy(C,Before,sizeof(C));C[11]^=1;Reject(C,"different captured subsystem rejected");
  memcpy(C,Before,sizeof(C));C[1]&=~2U;Reject(C,"captured MEM-off state rejected");
  memcpy(C,Before,sizeof(C));C[1]|=4U;Reject(C,"captured BME-on state rejected");
  Check(!GuCheckConfig(Before,GU_TARGET_SEGMENT^1U,GU_TARGET_BUS,0,0,&Caps),"captured config on another segment rejected");
  Check(!GuCheckConfig(Before,GU_TARGET_SEGMENT,GU_TARGET_BUS+1U,0,0,&Caps),"captured config on another bus rejected");
  Check(!GuCheckConfig(Before,GU_TARGET_SEGMENT,GU_TARGET_BUS,1,0,&Caps),"captured config on another device rejected");
  Check(!GuCheckConfig(Before,GU_TARGET_SEGMENT,GU_TARGET_BUS,0,1,&Caps),"captured config on another function rejected");
}

static void LegacyCapture(UINT32 C[64]) {
  Basic(C);memset(C+16,0,48*sizeof(*C));C[0x34/4]=0x60;
  C[0x60/4]=0x00036801;C[0x68/4]=0x00807805;
  C[0x78/4]=0x0012b410;C[0x7c/4]=1U<<28;
  C[0xb4/4]=0x00140009;C[0xc8/4]=0x00050011;
}

static void TestIdentity(void) {
  static const struct { unsigned Offset, Value; const char *Name; } Mutations[] = {
    {0, 0xdf, "wrong PCI vendor rejected"},
    {2, 0x07, "wrong PCI device rejected"},
    {0x2c, 0x63, "wrong subsystem vendor rejected"},
    {0x2e, 0x91, "wrong subsystem device rejected"},
    {0x0b, 0x04, "non-display class rejected"},
    {0x0e, 0x81, "bridge header rejected"},
    {0x04, 0x00, "memory decode off rejected"},
    {0x04, 0x06, "bus mastering on rejected"},
    {0x30, 0x01, "ROM decode on rejected"},
    {0x06, 0x00, "absent capability-list status rejected"},
    {0x34, 0x00, "missing first capability rejected"},
    {0x34, 0x3c, "first capability in header rejected"},
    {0x34, 0x41, "unaligned first capability rejected"},
    {0x34, 0xff, "out-of-range first capability rejected"}
  };
  UINT32 C[64];
  GU_CAPS Caps;
  unsigned I;
  Basic(C);
  Check(Valid(C), "minimal PM and PCIe config accepted");
  C[3] = 0;
  Check(Valid(C), "single-function normal header accepted");
  for (I = 0; I < sizeof(Mutations) / sizeof(Mutations[0]); ++I) {
    Basic(C);
    /* A LOCAL subsystem byte may already equal an old fixture's negative value. */
    unsigned Value=Mutations[I].Value;
    if(Mutations[I].Offset==0 || Mutations[I].Offset==2 || Mutations[I].Offset==0x2c || Mutations[I].Offset==0x2e)
      Value=ConfigByte(C,Mutations[I].Offset)^1U;
    Byte(C, Mutations[I].Offset, Value);
    Reject(C, Mutations[I].Name);
  }
  Basic(C);
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT^1U, GU_TARGET_BUS, 0, 0, &Caps), "wrong segment rejected");
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT+0x10000U, GU_TARGET_BUS, 0, 0, &Caps), "out-of-range segment is not truncated into the target");
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS + 1, 0, 0, &Caps), "wrong bus rejected");
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 1, 0, &Caps), "wrong device location rejected");
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 1, &Caps), "wrong function location rejected");
  Check(!GuCheckConfig(NULL, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps), "null config rejected");
  Check(!GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, NULL), "null output rejected");
}

static void TestPowerAndPcie(void) {
  UINT32 C[64];
  unsigned I;
  for (I = 1; I <= 3; ++I) {
    Basic(C);
    Word(C, 0x44, I);
    Reject(C, "PM states D1/D2/D3 rejected");
  }
  Basic(C); Byte(C, 0x34, 0x48);
  Reject(C, "missing PM rejected");
  Basic(C); Byte(C, 0x41, 0);
  Reject(C, "missing PCIe rejected");
  Basic(C); Word(C, 0x42, 0);
  Reject(C, "reserved PM version rejected");
  Basic(C); Word(C, 0x42, 4);
  Reject(C, "unsupported PM version rejected");
  for (I = 1; I <= 2; ++I) {
    Basic(C); Word(C, 0x4a, I);
    Check(Valid(C), "PCIe endpoint version 1/2 accepted");
    Word(C, 0x4a, 0x10 | I);
    Check(Valid(C), "PCIe legacy endpoint version 1/2 accepted");
  }
  Basic(C); Word(C, 0x4a, 0);
  Reject(C, "PCIe version zero rejected");
  Basic(C); Word(C, 0x4a, 3);
  Reject(C, "unsupported PCIe version rejected");
  for (I = 2; I <= 15; ++I) {
    Basic(C); Word(C, 0x4a, (I << 4) | 2);
    Reject(C, "non-endpoint PCIe type rejected");
  }
  Basic(C); C[0x4c / 4] = 0;
  Reject(C, "absent FLR support rejected");
  Basic(C); Word(C, 0x50, 0x8000);
  Reject(C, "asserted FLR initiate bit rejected");
}

static void TestCapabilities(void) {
  UINT32 C[64];
  GU_CAPS Caps;
  unsigned I;
  static const unsigned BadPointers[] = {0x3c, 0x41, 0x49, 0xff};
  static const unsigned MsiControl[] = {0, 0x80, 0x100, 0x180};
  for (I = 0; I < sizeof(BadPointers) / sizeof(BadPointers[0]); ++I) {
    Basic(C); Byte(C, 0x41, BadPointers[I]);
    Reject(C, "malformed next capability pointer rejected");
  }
  Basic(C); Byte(C, 0x41, 0x40);
  Reject(C, "self-referential capability rejected");
  Basic(C); Byte(C, 0x49, 0x40);
  Reject(C, "cyclic capability list rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x00030001;
  Reject(C, "duplicate PM rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x00020010;
  C[0x94 / 4] = 0x10000000;
  Reject(C, "duplicate PCIe rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x0000b005;
  C[0xb0 / 4] = 0x00000005;
  Reject(C, "duplicate MSI rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x0000b011;
  C[0xb0 / 4] = 0x00000011;
  Reject(C, "duplicate MSI-X rejected");
  for (I = 0; I < sizeof(MsiControl) / sizeof(MsiControl[0]); ++I) {
    Basic(C); Byte(C, 0x49, 0x90);
    C[0x90 / 4] = (MsiControl[I] << 16) | 5;
    Check(GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps) && Caps.msi == 0x90,
          "disabled 32/64-bit MSI with/without masks accepted");
    Word(C, 0x92, MsiControl[I] | 1);
    Reject(C, "enabled MSI rejected for each layout");
  }
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x40000011;
  Check(GuCheckConfig(C, GU_TARGET_SEGMENT, GU_TARGET_BUS, 0, 0, &Caps) && Caps.msix == 0x90,
        "disabled and function-masked MSI-X accepted");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x00040009;
  Check(Valid(C), "bounded vendor capability accepted");
  Byte(C, 0x92, 0);
  Reject(C, "zero vendor capability length rejected");
  Byte(C, 0x92, 2);
  Reject(C, "short vendor capability length rejected");
  Byte(C, 0x92, 0x71);
  Reject(C, "vendor capability past config end rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x000000ff;
  Reject(C, "unsupported capability format fails closed");
  Basic(C); Byte(C, 0x49, 0x50); C[0x50 / 4] = 0x00040009;
  Reject(C, "capability overlapping PCIe body rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x0020a009;
  C[0xa0 / 4] = 0x00000005;
  Reject(C, "capability overlapping vendor body rejected");
  Basic(C); Byte(C, 0x49, 0x90); C[0x90 / 4] = 0x0180a405;
  C[0xa4 / 4] = 0x00000011;
  Reject(C, "capability overlapping MSI pending-mask field rejected");
  Basic(C); Byte(C, 0x34, 0xfc); C[0xfc / 4] = 0x00030001;
  Reject(C, "truncated PM at end of config rejected");
  Basic(C); Byte(C, 0x41, 0xc4); C[0xc4 / 4] = 0x00020010;
  C[0xc8 / 4] = 0x10000000;
  Check(Valid(C), "PCIe v2 ending at byte 256 accepted");
  Byte(C, 0x41, 0xc8); C[0xc8 / 4] = 0x00020010;
  C[0xcc / 4] = 0x10000000;
  Reject(C, "PCIe v2 crossing byte 256 rejected");
  Basic(C); Byte(C, 0x41, 0xec); C[0xec / 4] = 0x00010010;
  C[0xf0 / 4] = 0x10000000;
  Check(Valid(C), "PCIe v1 ending at byte 256 accepted");
  Byte(C, 0x41, 0xf0); C[0xf0 / 4] = 0x00010010;
  C[0xf4 / 4] = 0x10000000;
  Reject(C, "PCIe v1 crossing byte 256 rejected");
  Basic(C); Byte(C, 0x49, 0xf4); C[0xf4 / 4] = 0x00000005;
  Check(Valid(C), "32-bit MSI fitting final 12 bytes accepted");
  Word(C, 0xf6, 0x80);
  Reject(C, "64-bit MSI truncated at end rejected");
  Word(C, 0xf6, 0x100);
  Reject(C, "MSI mask registers truncated at end rejected");
  Basic(C); Byte(C, 0x49, 0xf4); C[0xf4 / 4] = 0x00000011;
  Check(Valid(C), "MSI-X ending at byte 256 accepted");
  Byte(C, 0x49, 0xf8); C[0xf8 / 4] = 0x00000011;
  Reject(C, "MSI-X crossing byte 256 rejected");
  Basic(C); Byte(C, 0x49, 0xfc); C[0xfc / 4] = 0x00040009;
  Check(Valid(C), "vendor capability ending at byte 256 accepted");
}

static const UINT8 Intel[] = {
  2,1,12,0, 0xd0,0x41,3,0x0a, 0,0,0,0,
  1,1,6,0, 0,2,
  2,3,8,0, 0x11,0x43,1,0x80,
  0x7f,0xff,4,0
};
/* Root port 1 with the function this profile does not use: Pci(1,0) for root port 1.1
   profiles (unchanged), Pci(1,1) for root port 1.0 profiles, whose real route is Pci(1,0). */
static const UINT8 Rtx[] = {
  2,1,12,0, 0xd0,0x41,3,0x0a, 0,0,0,0,
  1,1,6,0, GU_ROOT_PORT_FUNCTION ? 0 : 1,1,
  1,1,6,0, 0,0,
  0x7f,0xff,4,0
};

static int Path(const UINT8 *Bytes, UINTN Length) {
  return GuDisplayPath((const EFI_DEVICE_PATH_PROTOCOL *)Bytes, Length);
}

static void LegacyTestPaths(void) {
  UINT8 P[96];
  unsigned I;
  static const struct { unsigned Offset, Value; const char *Name; } Mutations[] = {
    {0, 1, "non-ACPI root rejected"},
    {1, 2, "expanded ACPI root rejected"},
    {2, 0, "zero root length rejected"},
    {2, 11, "short root length rejected"},
    {2, 13, "long root length rejected"},
    {3, 1, "oversized root length rejected"},
    {4, 0xd1, "non-PNP root rejected"},
    {6, 4, "unknown root HID rejected"},
    {8, 1, "nonzero root UID rejected"},
    {12, 3, "alien node before PCI route rejected"},
    {13, 2, "non-PCI hardware route rejected"},
    {14, 5, "short PCI node rejected"},
    {14, 7, "long PCI node rejected"},
    {16, 1, "Intel nonzero function rejected"},
    {17, 3, "unknown root PCI device rejected"},
    {18, 3, "alien trailing node rejected"},
    {19, 1, "non-ADR trailing ACPI node rejected"},
    {20, 4, "ADR without an address rejected"},
    {20, 7, "partial ADR address rejected"},
    {20, 9, "unaligned ADR length rejected"},
    {20, 0xff, "oversized ADR rejected"},
    {27, 1, "multi-instance terminator rejected"},
    {28, 0, "zero end length rejected"},
    {28, 3, "short end length rejected"},
    {28, 5, "long end length rejected"}
  };
  Check(Path(Intel, sizeof(Intel)) == 1, "captured Intel GOP path accepted");
  Check(Path(Rtx, sizeof(Rtx)) == 2, "captured RTX PCI path accepted");
  Check(GuDisplayPath(NULL, 100) == 0, "null display path rejected");
  for (I = 0; I < sizeof(Intel); ++I)
    Check(Path(Intel, I) == 0, "every truncated Intel path rejected");
  for (I = 0; I < sizeof(Rtx); ++I)
    Check(Path(Rtx, I) == 0, "every truncated RTX path rejected");
  for (I = 0; I < sizeof(Mutations) / sizeof(Mutations[0]); ++I) {
    memcpy(P, Intel, sizeof(Intel));
    P[Mutations[I].Offset] = (UINT8)Mutations[I].Value;
    Check(Path(P, sizeof(Intel)) == 0, Mutations[I].Name);
  }
  memcpy(P, Intel, 18);
  memcpy(P + 18, Intel + 26, 4);
  Check(Path(P, 22) == 1, "Intel route without ADR accepted");
  P[6] = 8;
  Check(Path(P, 22) == 1, "PNP0A08 root accepted");
  memcpy(P, Rtx, 24);
  memcpy(P + 24, Intel + 18, 12);
  P[28] = 0x40; P[29] = 0x03;
  Check(Path(P, 36) == 2, "RTX route with output ADR accepted");
  memcpy(P + 32, Intel + 18, 12);
  Check(Path(P, 44) == 2, "multiple trailing ADR nodes accepted");
  memcpy(P + 32, P + 28, 4);
  P[26] = 12;
  memcpy(P + 36, Intel + 26, 4);
  Check(Path(P, 40) == 2, "multiple ADR entries in one node accepted");
  memset(P + 40, 0xa5, sizeof(P) - 40);
  Check(Path(P, sizeof(P)) == 2, "read bound may extend after end-entire");
  memcpy(P, Rtx, sizeof(Rtx)); P[22] = 1;
  Check(Path(P, sizeof(Rtx)) == 0, "RTX nonzero endpoint function rejected");
  memcpy(P, Rtx, sizeof(Rtx)); P[23] = 1;
  Check(Path(P, sizeof(Rtx)) == 0, "RTX wrong downstream device rejected");
  memcpy(P, Rtx, 18); memcpy(P + 18, Rtx + 24, 4);
  Check(Path(P, 22) == 0, "bridge-only RTX route rejected");
  memcpy(P, Rtx, 12); memcpy(P + 12, Rtx + 24, 4);
  Check(Path(P, 16) == 0, "root-only path rejected");
  memcpy(P, Intel, 12); memcpy(P + 12, Intel + 18, 12);
  Check(Path(P, 24) == 0, "ADR before PCI rejected");
  memcpy(P, Intel, 18); memcpy(P + 18, Rtx + 18, 10);
  Check(Path(P, 28) == 0, "additional PCI node after Intel route rejected");
  memcpy(P, Intel, 18); memcpy(P + 18, Intel, 12); memcpy(P + 30, Intel + 26, 4);
  Check(Path(P, 34) == 0, "second ACPI root rejected");
  memset(P, 0, sizeof(P));
  Check(Path(P, sizeof(P)) == 0, "zero-filled path rejected");
}

static void TestPaths(void) {
  static const UINT8 Amd[]={2,1,12,0,0xd0,0x41,3,0xa,0,0,0,0,
    1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,0,3,1,0x80,0x7f,0xff,4,0};
  UINT8 P[sizeof(Amd)];
  Check(Path(Amd,sizeof(Amd))==1,"exact RUN00 AMD GOP path accepted");
  Check(Path(Intel,sizeof(Intel))==0,"old Intel GOP rejected");
  Check(Path(Rtx,sizeof(Rtx))==0,"old RTX route function rejected");
  Check(!GuDisplayPath(NULL,256),"null path rejected");
  for(unsigned i=0;i<sizeof(Amd);i++) {
    Check(!Path(Amd,i),"each truncated AMD GOP rejected");
    memcpy(P,Amd,sizeof(P));P[i]^=0x40;
    Check(!Path(P,sizeof(P)),"each changed AMD GOP byte rejected");
  }
  memcpy(P,Amd,sizeof(P));P[16]=0;Check(!Path(P,sizeof(P)),"wrong bridge function rejected");
  memcpy(P,Amd,sizeof(P));P[6]=8;Check(!Path(P,sizeof(P)),"uncaptured ACPI root rejected");
  memcpy(P,Amd,24);memcpy(P+24,Amd+32,4);
  Check(!Path(P,28),"AMD GOP missing ADR rejected");
  (void)LegacyTestPaths; /* Preserved historical vectors, not this board's contract. */
}

#ifdef GU_PROFILE_LOCAL
static int ExactAmd(const UINT8 *p,UINTN bytes) {
  static const UINT8 Amd[]={2,1,12,0,0xd0,0x41,3,0x0a,0,0,0,0,
    1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,0,3,1,0x80,0x7f,0xff,4,0};
  return bytes>=sizeof(Amd) && !memcmp(p,Amd,sizeof(Amd));
}
static void TestLocalPaths(void) {
  static const UINT8 Target[]={GU_RTX_PATH_BYTES};
  static const UINT8 Amd[]={2,1,12,0,0xd0,0x41,3,0x0a,0,0,0,0,
    1,1,6,0,1,8,1,1,6,0,0,0,2,3,8,0,0,3,1,0x80,0x7f,0xff,4,0};
  UINT8 changed[sizeof(Target)+16];
  int expected=ExactAmd(Target,sizeof(Target))?1:2;
  Check(Path(Amd,sizeof(Amd))==1,"LOCAL preserves exact historical AMD classification");
  Check(Path(Target,sizeof(Target))==expected,"LOCAL classifies its complete captured PCI ancestry");
  Check(!GuDisplayPath(NULL,256),"LOCAL null path rejected");
  for(UINTN i=0;i<sizeof(Target);i++) {
    Check(!Path(Target,i),"every truncated LOCAL target rejected");
    memcpy(changed,Target,sizeof(Target));changed[i]^=0x40;
    int want=(i>=sizeof(Target)-8 && i<sizeof(Target)-4)?2:0;
    if(ExactAmd(changed,sizeof(Target)))want=1;
    Check(Path(changed,sizeof(Target))==want,"LOCAL pins ancestry and shape while discovering output ADR");
  }
  memset(changed,0xa5,sizeof(changed));memcpy(changed,Target,sizeof(Target));
  Check(Path(changed,sizeof(changed))==expected,"LOCAL bound may extend after end-entire");
  (void)TestPaths; /* Fixed-profile vectors remain unchanged in their own builds. */
}
#endif

int main(int Argc, char **Argv) {
  UINT32 Captured[64],Legacy[64];
  if (Argc != 2 || !LoadCapture(Argv[1], Captured)) return 2;
  TestCapture(Captured);
  LegacyCapture(Legacy);TestLegacyCapture(Legacy);
  TestIdentity();
  TestPowerAndPcie();
  TestCapabilities();
#ifdef GU_PROFILE_LOCAL
  TestLocalPaths();
#else
  TestPaths();
#endif
  if (Failures != 0) {
    fprintf(stderr, "%u/%u checks failed\n", Failures, Checks);
    return 1;
  }
  printf("PASS %u preflight checks (actual config, legacy capability layout and malformed inputs)\n", Checks);
  return 0;
}
