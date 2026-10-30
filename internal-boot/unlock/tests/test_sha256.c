/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ #include "../sha256.h"
#include "../machine-profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned Checks;
static unsigned Failures;

static const char EmptyHash[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
static const char AbcHash[] =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

static void Check(const char *Name, int Passed)
{
    ++Checks;
    if (!Passed) {
        ++Failures;
        printf("FAIL %s\n", Name);
    } else {
        printf("PASS %s\n", Name);
    }
}

static void ToHex(const UINT8 Digest[32], char Hex[65])
{
    static const char Digits[] = "0123456789abcdef";
    unsigned Index;

    for (Index = 0; Index < 32; ++Index) {
        Hex[2 * Index] = Digits[Digest[Index] >> 4];
        Hex[2 * Index + 1] = Digits[Digest[Index] & 15];
    }
    Hex[64] = '\0';
}

/* Expected values are fixed NIST vectors or independent hashlib results. */
static void CheckDigest(const char *Name, const void *Data, size_t Size,
                        const char *Expected)
{
    UINT8 Guarded[34];
    char Actual[65];
    char Label[160];

    memset(Guarded, 0xa5, sizeof(Guarded));
    GuSha256(Data, (UINTN)Size, Guarded + 1);
    ToHex(Guarded + 1, Actual);
    Check(Name, strcmp(Actual, Expected) == 0);
    if (strcmp(Actual, Expected) != 0) {
        printf("  expected %s\n  actual   %s\n", Expected, Actual);
    }
    snprintf(Label, sizeof(Label), "%s writes exactly 32 bytes", Name);
    Check(Label, Guarded[0] == 0xa5 && Guarded[33] == 0xa5);
}

static void TestNistVectors(void)
{
    static const char MultiBlock[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    static const char LongerMessage[] =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
        "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    static UINT8 MillionA[1000000];

    CheckDigest("empty", NULL, 0, EmptyHash);
    CheckDigest("abc", "abc", 3, AbcHash);
    CheckDigest("NIST 56-byte multiblock", MultiBlock,
                sizeof(MultiBlock) - 1,
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CheckDigest("NIST 112-byte multiblock", LongerMessage,
                sizeof(LongerMessage) - 1,
                "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    memset(MillionA, 'a', sizeof(MillionA));
    CheckDigest("NIST million a", MillionA, sizeof(MillionA),
                "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

static void TestPaddingAndBinaryInput(void)
{
    /* hashlib.sha256(bytes(range(length))).hexdigest(), generated independently. */
    static const struct {
        size_t Size;
        const char *Hex;
    } Cases[] = {
        {1, "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d"},
        {55, "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
        {56, "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
        {63, "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
        {64, "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
        {65, "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"},
        {119, "da18797ed7c3a777f0847f429724a2d8cd5138e6ed2895c3fa1a6d39d18f7ec6"},
        {120, "f52b23db1fbb6ded89ef42a23ce0c8922c45f25c50b568a93bf1c075420bbb7c"},
        {127, "92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976"},
        {128, "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5"},
        {129, "5099c6a56203f9687f7d33f4bfdf576d31dc91f6b695ecea38b2770c87631135"}
    };
    UINT8 Unaligned[130];
    UINT8 Original[130];
    char Label[80];
    size_t Index;

    Unaligned[0] = 0xa5;
    for (Index = 1; Index < sizeof(Unaligned); ++Index) {
        Unaligned[Index] = (UINT8)(Index - 1);
    }
    memcpy(Original, Unaligned, sizeof(Original));
    for (Index = 0; Index < sizeof(Cases) / sizeof(Cases[0]); ++Index) {
        snprintf(Label, sizeof(Label), "unaligned binary length %zu", Cases[Index].Size);
        CheckDigest(Label, Unaligned + 1, Cases[Index].Size, Cases[Index].Hex);
    }
    Check("hashing preserves input bytes",
          memcmp(Unaligned, Original, sizeof(Original)) == 0);
}

static void TestHashMatching(void)
{
    static const unsigned char Invalid[] = {'g', 'G', '/', ':', '@', '`', ' ', '\n', 0x80};
    char Text[66];
    size_t Position;
    size_t InvalidIndex;
    int AllRejected;

    Check("matching lower-case hex", GuHashMatches("abc", 3, AbcHash));
    Check("matching upper-case hex", GuHashMatches("abc", 3,
          "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));
    Check("matching mixed-case hex", GuHashMatches("abc", 3,
          "bA7816bF8f01cFeA414140dE5dAe2223b00361a396177A9cB410fF61f20015Ad"));
    Check("matching empty data", GuHashMatches(NULL, 0, EmptyHash));
    Check("reject NULL hex", !GuHashMatches("abc", 3, NULL));
    Check("reject NULL nonempty data", !GuHashMatches(NULL, 3, AbcHash));
    Check("reject message length outside SHA-256 domain",
          !GuHashMatches("abc", (UINTN)1 << 61, AbcHash));

    AllRejected = 1;
    for (Position = 0; Position < 64; ++Position) {
        memcpy(Text, AbcHash, 65);
        Text[Position] = Text[Position] == '0' ? '1' : '0';
        if (GuHashMatches("abc", 3, Text)) {
            AllRejected = 0;
        }
    }
    Check("reject wrong digest at each of 64 hex positions", AllRejected);

    AllRejected = 1;
    for (Position = 0; Position < 64; ++Position) {
        memcpy(Text, AbcHash, 65);
        Text[Position] = '\0';
        if (GuHashMatches("abc", 3, Text)) {
            AllRejected = 0;
        }
    }
    Check("reject all truncated hex lengths 0 through 63", AllRejected);

    AllRejected = 1;
    for (InvalidIndex = 0; InvalidIndex < sizeof(Invalid); ++InvalidIndex) {
        for (Position = 0; Position < 64; ++Position) {
            memcpy(Text, AbcHash, 65);
            Text[Position] = (char)Invalid[InvalidIndex];
            if (GuHashMatches("abc", 3, Text)) {
                AllRejected = 0;
            }
        }
    }
    Check("reject non-hex characters at every position", AllRejected);
    memcpy(Text, AbcHash, 65);
    Text[64] = '0';
    Text[65] = '\0';
    Check("reject overlong hex", !GuHashMatches("abc", 3, Text));
}

static void TestPinnedFile(const char *Path, const char *Expected)
{
    FILE *File = fopen(Path, "rb");
    long Length;
    UINT8 *Data;
    UINT8 Digest[32];
    char Actual[65];
    char ExpectedLower[65];
    unsigned Index;

    if (File == NULL) {
        Check(Path, 0);
        return;
    }
    if (fseek(File, 0, SEEK_END) != 0 || (Length = ftell(File)) < 0 ||
        fseek(File, 0, SEEK_SET) != 0) {
        fclose(File);
        Check("read pinned file length", 0);
        return;
    }
    Data = malloc(Length == 0 ? 1 : (size_t)Length);
    if (Data == NULL) {
        fclose(File);
        Check("allocate hosted test file buffer", 0);
        return;
    }
    if (fread(Data, 1, (size_t)Length, File) != (size_t)Length) {
        fclose(File);
        free(Data);
        Check("read pinned file bytes", 0);
        return;
    }
    fclose(File);
    GuSha256(Data, (UINTN)Length, Digest);
    ToHex(Digest, Actual);
    for (Index = 0; Index < 64; ++Index) {
        ExpectedLower[Index] = Expected[Index] >= 'A' && Expected[Index] <= 'F'
            ? (char)(Expected[Index] - 'A' + 'a') : Expected[Index];
    }
    ExpectedLower[64] = '\0';
    Check(Path, strcmp(Actual, ExpectedLower) == 0);
    printf("  bytes=%ld sha256=%s\n", Length, Actual);
    Check("GuHashMatches pinned file", GuHashMatches(Data, (UINTN)Length, Expected));
    free(Data);
}

int main(int Argc, char **Argv)
{
    if (Argc != 2 && Argc != 3) {
        fprintf(stderr, "usage: %s [gsp_ga10x.bin] PROM.BIN\n", Argv[0]);
        return 2;
    }
    TestNistVectors();
    TestPaddingAndBinaryInput();
    TestHashMatching();
    if (Argc == 3) TestPinnedFile(Argv[1],
        "572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a");
    TestPinnedFile(Argv[Argc - 1], GU_PROM_SHA256);
    printf("SHA-256: %u checks, %u failures\n", Checks, Failures);
    return Failures != 0;
}
