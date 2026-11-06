/*
 * hashsum - 파일의 CRC32 / MD5 / SHA-1 / SHA-256 해시값을 출력하는 CLI 도구
 *
 * 사용법 : hashsum.exe [옵션] <파일> [파일2 ...]   (옵션은 hashsum.exe /? 참고)
 * 대상   : Windows 7 이상, 32/64비트 (KERNEL32.dll, SHELL32.dll 만 사용)
 * 빌드   : build.bat (Windows) 또는 build.sh (Linux/macOS 에서 크로스 빌드)
 *
 * - 유니코드(한글 등) 파일명 지원
 * - 4GB 이상 대용량 파일 지원 (파일을 한 번만 읽어 선택한 해시를 동시에 계산)
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>

/*
 * C 런타임(CRT) 없이 빌드: Windows 7 기본 설치에 없는 UCRT 의존성 제거
 *
 * 컴파일러가 구조체 초기화 등에서 암묵적으로 호출할 수 있는 memcpy / memset 을
 * 직접 제공한다. volatile 은 최적화기가 이 루프를 다시 memcpy/memset 호출로
 * 바꿔 자기 자신을 부르는 것을 막기 위함.
 *
 * CRT 없이 빌드하려면 다음도 지켜야 한다 (x86 에서 CRT 보조 함수 호출 방지)
 *   - 64비트 정수의 나눗셈 / 변수만큼의 시프트를 쓰지 않는다 (상수 시프트만 사용)
 *   - 4KB 를 넘는 지역 변수를 두지 않는다 (__chkstk 호출 방지)
 */
#if defined(_MSC_VER) && !defined(__clang__)
#pragma function(memcpy, memset)
#endif
void *memcpy(void *d, const void *s, size_t n)
{
    volatile uint8_t *dp = (volatile uint8_t *)d;
    const uint8_t *sp = (const uint8_t *)s;
    while (n--) *dp++ = *sp++;
    return d;
}
void *memset(void *d, int c, size_t n)
{
    volatile uint8_t *dp = (volatile uint8_t *)d;
    while (n--) *dp++ = (uint8_t)c;
    return d;
}
static int wcs_eq(const wchar_t *a, const wchar_t *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
/* 옵션 비교용: 영문 대소문자 구분 없음 (/V, /SHA256 도 인식) */
static int wcs_ieq(const wchar_t *a, const wchar_t *b)
{
    for (;; a++, b++) {
        wchar_t x = *a, y = *b;
        if (x >= L'A' && x <= L'Z') x = (wchar_t)(x + 32);
        if (y >= L'A' && y <= L'Z') y = (wchar_t)(y + 32);
        if (x != y) return 0;
        if (!x) return 1;
    }
}
static int wcs_len(const wchar_t *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}
/* 64비트 정수 -> 10진 문자열 (32비트 연산만 사용) */
static void u64_to_dec(uint64_t v, char *out)
{
    char tmp[24];
    int n = 0, i;
    uint32_t hi = (uint32_t)(v >> 32), lo = (uint32_t)v;
    do {
        uint32_t rh = hi % 10, q_hi = hi / 10;
        uint32_t mid = (rh << 16) | (lo >> 16);
        uint32_t q_mid = mid / 10, r_mid = mid % 10;
        uint32_t low = (r_mid << 16) | (lo & 0xFFFF);
        uint32_t q_low = low / 10;
        tmp[n++] = (char)('0' + low % 10);
        hi = q_hi;
        lo = (q_mid << 16) | q_low;
    } while (hi || lo);
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}
static char *str_cat(char *d, const char *s)
{
    while (*s) *d++ = *s++;
    *d = 0;
    return d;
}

#define VERSION "1.6"
#define BUF_SIZE (1024 * 1024)

/* ======================= CRC32 (IEEE 802.3) ======================= */
static uint32_t crc_table[256];

static void crc32_init_table(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table[i] = c;
    }
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
    crc = ~crc;
    while (n--)
        crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* ============================== MD5 =============================== */
typedef struct {
    uint32_t s[4];
    uint64_t len;
    uint8_t  buf[64];
    size_t   used;
} md5_ctx;

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static const uint32_t md5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const uint8_t md5_R[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

static void md5_block(md5_ctx *c, const uint8_t *p)
{
    uint32_t M[16], a = c->s[0], b = c->s[1], cc = c->s[2], d = c->s[3], f, g, t;
    int i;
    for (i = 0; i < 16; i++)
        M[i] = (uint32_t)p[i*4] | ((uint32_t)p[i*4+1] << 8) |
               ((uint32_t)p[i*4+2] << 16) | ((uint32_t)p[i*4+3] << 24);
    for (i = 0; i < 64; i++) {
        if (i < 16)      { f = (b & cc) | (~b & d);  g = i; }
        else if (i < 32) { f = (d & b) | (~d & cc);  g = (5*i + 1) & 15; }
        else if (i < 48) { f = b ^ cc ^ d;           g = (3*i + 5) & 15; }
        else             { f = cc ^ (b | ~d);        g = (7*i) & 15; }
        t = d; d = cc; cc = b;
        b = b + ROTL(a + f + md5_K[i] + M[g], md5_R[i]);
        a = t;
    }
    c->s[0] += a; c->s[1] += b; c->s[2] += cc; c->s[3] += d;
}

static void md5_init(md5_ctx *c)
{
    c->s[0] = 0x67452301; c->s[1] = 0xefcdab89;
    c->s[2] = 0x98badcfe; c->s[3] = 0x10325476;
    c->len = 0; c->used = 0;
}

static void md5_update(md5_ctx *c, const uint8_t *p, size_t n)
{
    c->len += n;
    if (c->used) {
        size_t k = 64 - c->used;
        if (k > n) k = n;
        memcpy(c->buf + c->used, p, k);
        c->used += k; p += k; n -= k;
        if (c->used == 64) { md5_block(c, c->buf); c->used = 0; }
    }
    while (n >= 64) { md5_block(c, p); p += 64; n -= 64; }
    if (n) { memcpy(c->buf, p, n); c->used = n; }
}

static void md5_final(md5_ctx *c, uint8_t out[16])
{
    uint64_t bits = c->len << 3;
    uint8_t pad[72] = {0x80};
    size_t padlen = (c->used < 56) ? (56 - c->used) : (120 - c->used);
    uint8_t lenb[8];
    int i;
    for (i = 0; i < 8; i++) { lenb[i] = (uint8_t)bits; bits >>= 8; }
    md5_update(c, pad, padlen);
    md5_update(c, lenb, 8);
    for (i = 0; i < 4; i++) {
        out[i*4]   = (uint8_t)(c->s[i]);
        out[i*4+1] = (uint8_t)(c->s[i] >> 8);
        out[i*4+2] = (uint8_t)(c->s[i] >> 16);
        out[i*4+3] = (uint8_t)(c->s[i] >> 24);
    }
}

/* ============================= SHA-1 ============================== */
typedef struct {
    uint32_t s[5];
    uint64_t len;
    uint8_t  buf[64];
    size_t   used;
} sha1_ctx;

static void sha1_block(sha1_ctx *c, const uint8_t *p)
{
    uint32_t W[80], a, b, cc, d, e, f, k, t;
    int i;
    for (i = 0; i < 16; i++)
        W[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 80; i++)
        W[i] = ROTL(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1);
    a = c->s[0]; b = c->s[1]; cc = c->s[2]; d = c->s[3]; e = c->s[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & cc) | (~b & d);           k = 0x5a827999; }
        else if (i < 40) { f = b ^ cc ^ d;                    k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8f1bbcdc; }
        else             { f = b ^ cc ^ d;                    k = 0xca62c1d6; }
        t = ROTL(a, 5) + f + e + k + W[i];
        e = d; d = cc; cc = ROTL(b, 30); b = a; a = t;
    }
    c->s[0] += a; c->s[1] += b; c->s[2] += cc; c->s[3] += d; c->s[4] += e;
}

static void sha1_init(sha1_ctx *c)
{
    c->s[0] = 0x67452301; c->s[1] = 0xefcdab89; c->s[2] = 0x98badcfe;
    c->s[3] = 0x10325476; c->s[4] = 0xc3d2e1f0;
    c->len = 0; c->used = 0;
}

static void sha1_update(sha1_ctx *c, const uint8_t *p, size_t n)
{
    c->len += n;
    if (c->used) {
        size_t k = 64 - c->used;
        if (k > n) k = n;
        memcpy(c->buf + c->used, p, k);
        c->used += k; p += k; n -= k;
        if (c->used == 64) { sha1_block(c, c->buf); c->used = 0; }
    }
    while (n >= 64) { sha1_block(c, p); p += 64; n -= 64; }
    if (n) { memcpy(c->buf, p, n); c->used = n; }
}

static void sha1_final(sha1_ctx *c, uint8_t out[20])
{
    uint64_t bits = c->len << 3;
    uint8_t pad[72] = {0x80};
    size_t padlen = (c->used < 56) ? (56 - c->used) : (120 - c->used);
    uint8_t lenb[8];
    int i;
    for (i = 7; i >= 0; i--) { lenb[i] = (uint8_t)bits; bits >>= 8; }
    sha1_update(c, pad, padlen);
    sha1_update(c, lenb, 8);
    for (i = 0; i < 5; i++) {
        out[i*4]   = (uint8_t)(c->s[i] >> 24);
        out[i*4+1] = (uint8_t)(c->s[i] >> 16);
        out[i*4+2] = (uint8_t)(c->s[i] >> 8);
        out[i*4+3] = (uint8_t)(c->s[i]);
    }
}

/* ============================ SHA-256 ============================= */
typedef struct {
    uint32_t s[8];
    uint64_t len;
    uint8_t  buf[64];
    size_t   used;
} sha256_ctx;

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t sha_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_block(sha256_ctx *c, const uint8_t *p)
{
    uint32_t W[64], a, b, cc, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        W[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(W[i-15], 7) ^ ROTR(W[i-15], 18) ^ (W[i-15] >> 3);
        uint32_t s1 = ROTR(W[i-2], 17) ^ ROTR(W[i-2], 19) ^ (W[i-2] >> 10);
        W[i] = W[i-16] + s0 + W[i-7] + s1;
    }
    a = c->s[0]; b = c->s[1]; cc = c->s[2]; d = c->s[3];
    e = c->s[4]; f = c->s[5]; g = c->s[6]; h = c->s[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + sha_K[i] + W[i];
        t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->s[0] += a; c->s[1] += b; c->s[2] += cc; c->s[3] += d;
    c->s[4] += e; c->s[5] += f; c->s[6] += g; c->s[7] += h;
}

static void sha256_init(sha256_ctx *c)
{
    static const uint32_t iv[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(c->s, iv, sizeof iv);
    c->len = 0; c->used = 0;
}

static void sha256_update(sha256_ctx *c, const uint8_t *p, size_t n)
{
    c->len += n;
    if (c->used) {
        size_t k = 64 - c->used;
        if (k > n) k = n;
        memcpy(c->buf + c->used, p, k);
        c->used += k; p += k; n -= k;
        if (c->used == 64) { sha256_block(c, c->buf); c->used = 0; }
    }
    while (n >= 64) { sha256_block(c, p); p += 64; n -= 64; }
    if (n) { memcpy(c->buf, p, n); c->used = n; }
}

static void sha256_final(sha256_ctx *c, uint8_t out[32])
{
    uint64_t bits = c->len << 3;
    uint8_t pad[72] = {0x80};
    size_t padlen = (c->used < 56) ? (56 - c->used) : (120 - c->used);
    uint8_t lenb[8];
    int i;
    for (i = 7; i >= 0; i--) { lenb[i] = (uint8_t)bits; bits >>= 8; }
    sha256_update(c, pad, padlen);
    sha256_update(c, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(c->s[i] >> 24);
        out[i*4+1] = (uint8_t)(c->s[i] >> 16);
        out[i*4+2] = (uint8_t)(c->s[i] >> 8);
        out[i*4+3] = (uint8_t)(c->s[i]);
    }
}

/* =========================== 출력 도우미 =========================== */
static HANDLE g_out, g_err;

/* 콘솔이면 WriteConsoleW(한글 깨짐 없음), 리다이렉트면 UTF-8로 기록 */
static void write_w(HANDLE h, const wchar_t *s)
{
    DWORD mode, n;
    int len = wcs_len(s);
    if (len == 0) return;
    if (GetConsoleMode(h, &mode)) {
        WriteConsoleW(h, s, (DWORD)len, &n, NULL);
    } else {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, s, len, NULL, 0, NULL, NULL);
        char *buf = (char *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)bytes);
        if (!buf) return;
        WideCharToMultiByte(CP_UTF8, 0, s, len, buf, bytes, NULL, NULL);
        WriteFile(h, buf, (DWORD)bytes, &n, NULL);
        HeapFree(GetProcessHeap(), 0, buf);
    }
}

static void write_a(HANDLE h, const char *s)
{
    wchar_t w[256];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 256);
    write_w(h, w);
}

/* -g 옵션: 해시값을 4자리씩 공백으로 끊어서 출력 (기본은 붙여서 출력) */
static int g_group = 0;

static void to_hex(const uint8_t *d, size_t n, char *out)
{
    static const char hx[] = "0123456789abcdef";
    size_t i, o = 0;
    for (i = 0; i < n; i++) {
        if (g_group && i && (i % 2) == 0) out[o++] = ' ';
        out[o++] = hx[d[i] >> 4];
        out[o++] = hx[d[i] & 15];
    }
    out[o] = 0;
}

static void print_error(const wchar_t *path, DWORD err)
{
    wchar_t *msg = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, (LPWSTR)&msg, 0, NULL);
    write_w(g_err, L"오류: ");
    write_w(g_err, path);
    write_w(g_err, L" : ");
    if (msg) { write_w(g_err, msg); LocalFree(msg); }
    else write_w(g_err, L"파일을 읽을 수 없습니다.\r\n");
}

/* ============================ 파일 처리 ============================ */
/* 출력할 해시 선택 (0 이면 전부) */
#define SEL_CRC32  1
#define SEL_MD5    2
#define SEL_SHA1   4
#define SEL_SHA256 8
#define SEL_ALL    (SEL_CRC32 | SEL_MD5 | SEL_SHA1 | SEL_SHA256)
static int g_sel = 0;

static int hash_file(const wchar_t *path, uint8_t *buf)
{
    HANDLE f;
    DWORD got;
    uint32_t crc = 0;
    md5_ctx md5;
    sha1_ctx sha1;
    sha256_ctx sha;
    uint8_t md5d[16], sha1d[20], shad[32];
    char hex[96], line[160];
    LARGE_INTEGER size;

    /* 폴더는 CreateFileW 로 열면 '액세스 거부'로 실패하므로 먼저 확인한다 */
    {
        DWORD attr = GetFileAttributesW(path);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            write_w(g_err, L"오류: ");
            write_w(g_err, path);
            write_w(g_err, L" : 폴더입니다. 파일을 지정해 주세요.\r\n");
            return 0;
        }
    }

    f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) { print_error(path, GetLastError()); return 0; }

    md5_init(&md5);
    sha1_init(&sha1);
    sha256_init(&sha);
    size.QuadPart = 0;

    for (;;) {
        if (!ReadFile(f, buf, BUF_SIZE, &got, NULL)) {
            DWORD e = GetLastError();
            CloseHandle(f);
            print_error(path, e);
            return 0;
        }
        if (got == 0) break;
        if (g_sel & SEL_CRC32)  crc = crc32_update(crc, buf, got);
        if (g_sel & SEL_MD5)    md5_update(&md5, buf, got);
        if (g_sel & SEL_SHA1)   sha1_update(&sha1, buf, got);
        if (g_sel & SEL_SHA256) sha256_update(&sha, buf, got);
        size.QuadPart += got;
    }
    CloseHandle(f);

    md5_final(&md5, md5d);
    sha1_final(&sha1, sha1d);
    sha256_final(&sha, shad);

    write_w(g_out, L"File    : ");
    write_w(g_out, path);
    write_w(g_out, L"\r\n");

    {
        char num[24], *p;
        uint8_t crcb[4];
        crcb[0] = (uint8_t)(crc >> 24); crcb[1] = (uint8_t)(crc >> 16);
        crcb[2] = (uint8_t)(crc >> 8);  crcb[3] = (uint8_t)crc;

        u64_to_dec((uint64_t)size.QuadPart, num);
        p = str_cat(line, "Size    : "); p = str_cat(p, num); str_cat(p, " bytes\r\n");
        write_a(g_out, line);

        if (g_sel & SEL_CRC32) {
            to_hex(crcb, 4, hex);
            p = str_cat(line, "CRC32   : "); p = str_cat(p, hex); str_cat(p, "\r\n");
            write_a(g_out, line);
        }
        if (g_sel & SEL_MD5) {
            to_hex(md5d, 16, hex);
            p = str_cat(line, "MD5     : "); p = str_cat(p, hex); str_cat(p, "\r\n");
            write_a(g_out, line);
        }
        if (g_sel & SEL_SHA1) {
            to_hex(sha1d, 20, hex);
            p = str_cat(line, "SHA-1   : "); p = str_cat(p, hex); str_cat(p, "\r\n");
            write_a(g_out, line);
        }
        if (g_sel & SEL_SHA256) {
            to_hex(shad, 32, hex);
            p = str_cat(line, "SHA-256 : "); p = str_cat(p, hex); str_cat(p, "\r\n");
            write_a(g_out, line);
        }
    }
    return 1;
}

static void usage(void)
{
    write_w(g_out,
        L"hashsum " L"" VERSION L" - 파일의 CRC32 / MD5 / SHA-1 / SHA-256 해시값 출력\r\n"
        L"\r\n"
        L"사용법: hashsum.exe [옵션] <파일> [파일2 ...]\r\n"
        L"\r\n"
        L"옵션  : (/ 와 - 모두 사용 가능, 대소문자 구분 없음)\r\n"
        L"  /crc32,  -crc32    CRC32 해시값만 출력\r\n"
        L"  /md5,    -md5      MD5 해시값만 출력\r\n"
        L"  /sha1,   -sha1     SHA-1 해시값만 출력\r\n"
        L"  /sha256, -sha256   SHA-256 해시값만 출력\r\n"
        L"                     (여러 개를 함께 지정 가능, 지정하지 않으면 전부 출력)\r\n"
        L"  /g,      -g        해시값을 4자리씩 끊어서 출력 (예: 3524 41c2)\r\n"
        L"  /v,      -v        버전 표시\r\n"
        L"  /?, /h,  -h        도움말 출력\r\n"
        L"  --                 이후 인자는 모두 파일명으로 처리\r\n"
        L"\r\n"
        L"예시  : hashsum.exe setup.exe\r\n"
        L"        hashsum.exe /sha256 setup.exe\r\n"
        L"        hashsum.exe -md5 -sha1 -g setup.exe\r\n"
        L"        hashsum.exe \"C:\\My Files\\문서.zip\" data.bin\r\n");
}

/* 옵션 종류 */
enum { OPT_NONE, OPT_END, OPT_HELP, OPT_VERSION, OPT_GROUP, OPT_SELECT };

/* 인자가 옵션이면 종류를 돌려주고, 해시 선택 옵션이면 *sel 에 비트를 넣는다 */
static int parse_opt(const wchar_t *a, int *sel)
{
    static const struct { const wchar_t *name; int kind; int sel; } t[] = {
        { L"/?",      OPT_HELP,    0 }, { L"/h",      OPT_HELP,    0 },
        { L"-h",      OPT_HELP,    0 }, { L"--help",  OPT_HELP,    0 },
        { L"/v",      OPT_VERSION, 0 }, { L"-v",      OPT_VERSION, 0 },
        { L"--version", OPT_VERSION, 0 },
        { L"/g",      OPT_GROUP,   0 }, { L"-g",      OPT_GROUP,   0 },
        { L"--group", OPT_GROUP,   0 },
        { L"/crc32",  OPT_SELECT, SEL_CRC32  }, { L"-crc32",  OPT_SELECT, SEL_CRC32  },
        { L"/md5",    OPT_SELECT, SEL_MD5    }, { L"-md5",    OPT_SELECT, SEL_MD5    },
        { L"/sha1",   OPT_SELECT, SEL_SHA1   }, { L"-sha1",   OPT_SELECT, SEL_SHA1   },
        { L"/sha256", OPT_SELECT, SEL_SHA256 }, { L"-sha256", OPT_SELECT, SEL_SHA256 },
    };
    int i;
    if (wcs_eq(a, L"--")) return OPT_END;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        if (wcs_ieq(a, t[i].name)) {
            if (sel) *sel = t[i].sel;
            return t[i].kind;
        }
    }
    return OPT_NONE;
}

static int run(void)
{
    int argc, i, fail = 0, nfiles = 0, opts_done = 0;
    wchar_t **argv;
    uint8_t *buf;

    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    g_err = GetStdHandle(STD_ERROR_HANDLE);

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;

    /* 1단계: 옵션 해석 (옵션은 파일명 앞뒤 어디에 와도 됨) */
    for (i = 1; i < argc; i++) {
        int kind = OPT_NONE, sel = 0;
        if (!opts_done) kind = parse_opt(argv[i], &sel);
        switch (kind) {
        case OPT_END:     opts_done = 1; break;
        case OPT_HELP:    usage(); LocalFree(argv); return 0;
        case OPT_VERSION: write_w(g_out, L"hashsum " L"" VERSION L"\r\n"); LocalFree(argv); return 0;
        case OPT_GROUP:   g_group = 1; break;
        case OPT_SELECT:  g_sel |= sel; break;
        default:          nfiles++; break;
        }
    }
    if (nfiles == 0) {
        usage();
        LocalFree(argv);
        return 1;
    }
    if (g_sel == 0) g_sel = SEL_ALL;

    crc32_init_table();
    buf = (uint8_t *)VirtualAlloc(NULL, BUF_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) { write_w(g_err, L"오류: 메모리 부족\r\n"); return 2; }

    /* 2단계: 파일 처리 */
    opts_done = 0;
    nfiles = 0;
    for (i = 1; i < argc; i++) {
        const wchar_t *a = argv[i];
        if (!opts_done) {
            int kind = parse_opt(a, NULL);
            if (kind == OPT_END) { opts_done = 1; continue; }
            if (kind != OPT_NONE) continue;
        }
        if (nfiles++) write_w(g_out, L"\r\n");
        if (!hash_file(a, buf)) fail++;
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    LocalFree(argv);
    return fail ? 1 : 0;
}

/* CRT 없이 시작하는 진입점 */
void __stdcall entry(void)
{
    ExitProcess((UINT)run());
}
