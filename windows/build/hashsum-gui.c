/*
 * hashsum-gui - 파일의 CRC32 / MD5 / SHA-1 / SHA-256 해시값을 보여 주는 Windows 프로그램
 *
 * 대상   : Windows 7 이상, 32/64비트. 설치 없이 실행
 *          (KERNEL32, USER32, GDI32, SHELL32, COMDLG32, COMCTL32 만 사용)
 * 빌드   : build.bat (Windows) 또는 build.sh (Linux/macOS 에서 크로스 빌드)
 *          리소스 hashsum-gui.rc (아이콘, manifest, 버전 정보) 를 함께 링크
 *
 * 기능
 *   - 파일선택 버튼 또는 드래그 앤 드롭으로 파일 지정 (EXE 아이콘에 끌어다 놓아도 됨)
 *   - 출력 버튼: 별도 스레드에서 계산, 진행률 막대 표시, 취소 버튼으로 중단
 *   - 초기화 버튼: 선택한 파일과 결과를 모두 지움
 *   - 결과값은 마우스로 선택해 Ctrl+C 로 복사하거나, 옆의 복사 버튼으로 복사
 *
 * 해시 계산 코드는 명령줄 버전(hashsum.c)과 같다.
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
#include <commdlg.h>
#include <commctrl.h>
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

/* ============================ 화면 구성 ============================ */

#define APP_TITLE   L"hashsum"
#define BUF_SIZE    (1024 * 1024)
#define PATH_MAX_W  32768

/* 리소스 ID (hashsum-gui.rc 와 같아야 함) */
#define IDI_APP     1
#define IDI_COPY    2

/* 컨트롤 ID */
#define ID_PATH     100
#define ID_BROWSE   101
#define ID_RUN      IDOK        /* Enter 키 = 출력 */
#define ID_STOP     IDCANCEL    /* Esc 키 = 취소 */
#define ID_RESET    102
#define ID_PROGRESS 103
#define ID_RESULT0  110         /* 110..113 : 결과 칸 */
#define ID_COPY0    120         /* 120..123 : 복사 버튼 */

/* 작업 스레드 -> 창 메시지 */
#define WM_APP_PROGRESS (WM_APP + 1)   /* wParam = 진행률 (0..1000) */
#define WM_APP_DONE     (WM_APP + 2)   /* wParam = JOB_xxx, lParam = 오류 코드 */

#define JOB_OK        0
#define JOB_CANCELLED 1
#define JOB_ERROR     2
#define JOB_ISDIR     3

static const wchar_t *const LABELS[4] = { L"CRC32", L"MD5", L"SHA-1", L"SHA-256" };

static HINSTANCE g_inst;
static HWND g_wnd, g_path, g_browse, g_run, g_stop, g_reset, g_progress;
static HWND g_result[4], g_copy[4];
static HFONT g_font, g_mono;
static HBRUSH g_white;
static HICON g_copy_icon;
static int g_dpi = 96;
static int g_extra;      /* 해시값(64자)이 결과 칸에 다 들어가도록 늘린 폭 (화면 px) */

/* 작업 상태 */
static HANDLE g_thread;
static volatile LONG g_cancel;
static int g_busy;
static wchar_t g_job_path[PATH_MAX_W];
static wchar_t g_res[4][72];

/* 크기를 화면 배율(DPI)에 맞춤 (기준 96 DPI) */
static int S(int v) { return MulDiv(v, g_dpi, 96); }

/* ============================ 작업 스레드 ============================ */

static void to_hexw(const uint8_t *d, int n, wchar_t *out)
{
    static const wchar_t hx[] = L"0123456789abcdef";
    int i;
    for (i = 0; i < n; i++) {
        out[i*2]   = hx[d[i] >> 4];
        out[i*2+1] = hx[d[i] & 15];
    }
    out[n*2] = 0;
}

/* 진행률(0..1000) - 32비트 연산만 사용 (C 런타임의 64비트 나눗셈 함수를 피함) */
static int permille(uint64_t done, uint64_t total)
{
    uint32_t d, t;
    if (total == 0 || done >= total) return 1000;
    while (total > 0x3FFFFF) { total >>= 1; done >>= 1; }   /* done*1000 이 32비트를 넘지 않게 */
    d = (uint32_t)done; t = (uint32_t)total;
    return t ? (int)(d * 1000u / t) : 1000;
}

static DWORD WINAPI worker(LPVOID arg)
{
    HANDLE f;
    DWORD attr, got;
    LARGE_INTEGER sz;
    uint64_t total, done = 0;
    uint8_t *buf;
    uint32_t crc = 0;
    md5_ctx md5;
    sha1_ctx sha1;
    sha256_ctx sha;
    uint8_t md5d[16], sha1d[20], shad[32], crcb[4];
    int last = -1, p;
    (void)arg;

    attr = GetFileAttributesW(g_job_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        PostMessageW(g_wnd, WM_APP_DONE, JOB_ISDIR, 0);
        return 0;
    }
    f = CreateFileW(g_job_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        PostMessageW(g_wnd, WM_APP_DONE, JOB_ERROR, (LPARAM)GetLastError());
        return 0;
    }
    total = GetFileSizeEx(f, &sz) ? (uint64_t)sz.QuadPart : 0;

    buf = (uint8_t *)VirtualAlloc(NULL, BUF_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) {
        CloseHandle(f);
        PostMessageW(g_wnd, WM_APP_DONE, JOB_ERROR, (LPARAM)ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    md5_init(&md5);
    sha1_init(&sha1);
    sha256_init(&sha);

    for (;;) {
        if (g_cancel) {
            VirtualFree(buf, 0, MEM_RELEASE);
            CloseHandle(f);
            PostMessageW(g_wnd, WM_APP_DONE, JOB_CANCELLED, 0);
            return 0;
        }
        if (!ReadFile(f, buf, BUF_SIZE, &got, NULL)) {
            DWORD e = GetLastError();
            VirtualFree(buf, 0, MEM_RELEASE);
            CloseHandle(f);
            PostMessageW(g_wnd, WM_APP_DONE, JOB_ERROR, (LPARAM)e);
            return 0;
        }
        if (got == 0) break;
        crc = crc32_update(crc, buf, got);
        md5_update(&md5, buf, got);
        sha1_update(&sha1, buf, got);
        sha256_update(&sha, buf, got);
        done += got;
        p = permille(done, total);
        if (p != last) {                 /* 값이 바뀔 때만 알림 (최대 1000번) */
            last = p;
            PostMessageW(g_wnd, WM_APP_PROGRESS, (WPARAM)p, 0);
        }
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(f);

    md5_final(&md5, md5d);
    sha1_final(&sha1, sha1d);
    sha256_final(&sha, shad);
    crcb[0] = (uint8_t)(crc >> 24); crcb[1] = (uint8_t)(crc >> 16);
    crcb[2] = (uint8_t)(crc >> 8);  crcb[3] = (uint8_t)crc;

    to_hexw(crcb, 4, g_res[0]);
    to_hexw(md5d, 16, g_res[1]);
    to_hexw(sha1d, 20, g_res[2]);
    to_hexw(shad, 32, g_res[3]);
    PostMessageW(g_wnd, WM_APP_DONE, JOB_OK, 0);
    return 0;
}

/* ============================ 화면 동작 ============================ */

static void show_message(const wchar_t *text, const wchar_t *path, DWORD err)
{
    static wchar_t msg[PATH_MAX_W + 512];
    wchar_t *sys = NULL;
    int n = 0;
    const wchar_t *parts[3];
    int i;

    if (err) {
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0, (LPWSTR)&sys, 0, NULL);
    }
    parts[0] = text;
    parts[1] = path;
    parts[2] = sys;
    for (i = 0; i < 3; i++) {
        const wchar_t *s = parts[i];
        if (!s || !*s) continue;
        if (n) { msg[n++] = L'\r'; msg[n++] = L'\n'; msg[n++] = L'\r'; msg[n++] = L'\n'; }
        while (*s && n < (int)(sizeof msg / sizeof msg[0]) - 8) msg[n++] = *s++;
    }
    msg[n] = 0;
    if (sys) LocalFree(sys);
    MessageBoxW(g_wnd, msg, APP_TITLE, MB_OK | MB_ICONWARNING);
}

static void clear_results(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        SetWindowTextW(g_result[i], L"");
        EnableWindow(g_copy[i], FALSE);
    }
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
}

static void set_busy(int busy)
{
    g_busy = busy;
    EnableWindow(g_browse, !busy);
    EnableWindow(g_run, !busy);
    EnableWindow(g_reset, !busy);
    EnableWindow(g_stop, busy);
    SendMessageW(g_path, EM_SETREADONLY, (WPARAM)busy, 0);
    if (!busy) SetFocus(g_run);
}

/* 경로 칸 내용 바꾸기 (EN_CHANGE 에서 이전 결과가 지워짐) */
static void set_path(const wchar_t *p)
{
    SetWindowTextW(g_path, p);
    SendMessageW(g_path, EM_SETSEL, 0, -1);
}

/* 경로 칸의 글자를 읽고 앞뒤 공백과 따옴표를 뺌 (탐색기의 "경로로 복사" 는 따옴표를 붙임) */
static void read_path(wchar_t *out)
{
    int n, s = 0, e;
    n = GetWindowTextW(g_path, out, PATH_MAX_W);
    e = n;
    while (s < e && (out[s] == L' ' || out[s] == L'\t' || out[s] == L'"')) s++;
    while (e > s && (out[e-1] == L' ' || out[e-1] == L'\t' || out[e-1] == L'"')) e--;
    if (s > 0) { int i; for (i = 0; i < e - s; i++) out[i] = out[s + i]; }
    out[e - s] = 0;
}

static void start_job(void)
{
    DWORD tid;
    if (g_busy) return;
    read_path(g_job_path);
    if (!g_job_path[0]) {
        show_message(L"해시값을 계산할 파일을 선택해 주세요.", NULL, 0);
        return;
    }
    clear_results();
    g_cancel = 0;
    set_busy(1);
    g_thread = CreateThread(NULL, 0, worker, NULL, 0, &tid);
    if (!g_thread) {
        set_busy(0);
        show_message(L"작업을 시작할 수 없습니다.", NULL, GetLastError());
    }
}

static void job_done(WPARAM status, LPARAM err)
{
    int i;
    if (g_thread) {
        WaitForSingleObject(g_thread, INFINITE);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    set_busy(0);
    switch (status) {
    case JOB_OK:
        SendMessageW(g_progress, PBM_SETPOS, 1000, 0);
        for (i = 0; i < 4; i++) {
            SetWindowTextW(g_result[i], g_res[i]);
            EnableWindow(g_copy[i], TRUE);
        }
        break;
    case JOB_CANCELLED:
        SendMessageW(g_progress, PBM_SETPOS, 0, 0);
        break;
    case JOB_ISDIR:
        SendMessageW(g_progress, PBM_SETPOS, 0, 0);
        show_message(L"폴더는 해시값을 계산할 수 없습니다. 파일을 선택해 주세요.", g_job_path, 0);
        break;
    default:
        SendMessageW(g_progress, PBM_SETPOS, 0, 0);
        show_message(L"파일을 읽을 수 없습니다.", g_job_path, (DWORD)err);
        break;
    }
}

static void browse(void)
{
    static wchar_t file[PATH_MAX_W];
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof ofn);
    read_path(file);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = L"모든 파일 (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = PATH_MAX_W;
    ofn.lpstrTitle = L"파일 선택";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) set_path(file);
}

static void copy_result(int i)
{
    static wchar_t text[80];
    int n = GetWindowTextW(g_result[i], text, 80);
    HGLOBAL h;
    wchar_t *p;
    if (n <= 0) return;
    if (!OpenClipboard(g_wnd)) return;
    EmptyClipboard();
    h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)(n + 1) * sizeof(wchar_t));
    if (h) {
        p = (wchar_t *)GlobalLock(h);
        memcpy(p, text, (size_t)(n + 1) * sizeof(wchar_t));
        GlobalUnlock(h);
        if (!SetClipboardData(CF_UNICODETEXT, h)) GlobalFree(h);
    }
    CloseClipboard();
    /* 복사한 값을 선택 상태로 보여 줌 */
    SetFocus(g_result[i]);
    SendMessageW(g_result[i], EM_SETSEL, 0, -1);
}

static void on_drop(HDROP drop)
{
    static wchar_t file[PATH_MAX_W];
    if (!g_busy && DragQueryFileW(drop, 0, file, PATH_MAX_W)) set_path(file);
    DragFinish(drop);
}

/* ============================ 창 만들기 ============================ */

/* 컨트롤 만들기. 좌표는 96 DPI 기준.
 * grow: 0 = 그대로, WIDEN = 폭을 g_extra 만큼 늘림, SHIFT = 오른쪽으로 g_extra 만큼 이동 */
#define WIDEN 1
#define SHIFT 2
static HWND make(DWORD ex, const wchar_t *cls, const wchar_t *text, DWORD style,
                 int x, int y, int w, int h, int id, HFONT font, int grow)
{
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style,
                             S(x) + (grow == SHIFT ? g_extra : 0), S(y),
                             S(w) + (grow == WIDEN ? g_extra : 0), S(h),
                             g_wnd, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, FALSE);
    return c;
}

/*
 * 배치 (96 DPI 기준, 단위 px)
 *
 *   선택 파일
 *   [ 경로                                  ] [파일선택]
 *   [            출력            ] [ 취소 ] [ 초기화 ]
 *   [================ 진행률 막대 ===================]
 *   결과
 *   CRC32    [                                  ] [복사]
 *   MD5      [                                  ] [복사]
 *   SHA-1    [                                  ] [복사]
 *   SHA-256  [                                  ] [복사]
 */
#define CLIENT_W 620
#define CLIENT_H 330
#define MARGIN   18
#define RESULT_W (CLIENT_W - MARGIN - 36 - 8 - (MARGIN + 74))   /* 결과 칸 기본 폭 */

/* 결과 칸에 SHA-256(64자)이 다 보이도록 필요한 만큼 g_extra 를 정함 */
static void measure(void)
{
    static const wchar_t sample[] = L"0000000000000000000000000000000000000000000000000000000000000000";
    HDC dc = GetDC(NULL);
    HGDIOBJ old = SelectObject(dc, g_mono);
    SIZE sz;
    int need;
    sz.cx = 0;
    GetTextExtentPoint32W(dc, sample, 64, &sz);
    SelectObject(dc, old);
    ReleaseDC(NULL, dc);
    need = sz.cx + S(16);               /* 테두리와 안쪽 여백 */
    g_extra = need > S(RESULT_W) ? need - S(RESULT_W) : 0;
}

static void create_controls(void)
{
    HWND tip;
    TOOLINFOW ti;
    int i, y;
    const int right = CLIENT_W - MARGIN;           /* 오른쪽 끝 */
    const int bw = 100;                            /* 취소/초기화/파일선택 버튼 폭 */
    const int gap = 10;

    make(0, L"STATIC", L"선택 파일", 0, MARGIN, 16, 200, 20, 0, g_font, 0);
    g_path = make(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL,
                  MARGIN, 40, right - bw - gap - MARGIN, 28, ID_PATH, g_font, WIDEN);
    SendMessageW(g_path, EM_SETLIMITTEXT, PATH_MAX_W - 1, 0);
    g_browse = make(0, L"BUTTON", L"파일선택", WS_TABSTOP | BS_PUSHBUTTON,
                    right - bw, 40, bw, 28, ID_BROWSE, g_font, SHIFT);

    g_reset = make(0, L"BUTTON", L"초기화", WS_TABSTOP | BS_PUSHBUTTON,
                   right - bw, 82, bw, 32, ID_RESET, g_font, SHIFT);
    g_stop = make(0, L"BUTTON", L"취소", WS_TABSTOP | BS_PUSHBUTTON,
                  right - bw - gap - bw, 82, bw, 32, ID_STOP, g_font, SHIFT);
    g_run = make(0, L"BUTTON", L"출력", WS_TABSTOP | BS_DEFPUSHBUTTON,
                 MARGIN, 82, right - 2 * (bw + gap) - MARGIN, 32, ID_RUN, g_font, WIDEN);

    g_progress = make(0, PROGRESS_CLASSW, L"", PBS_SMOOTH, MARGIN, 124, right - MARGIN, 18,
                      ID_PROGRESS, g_font, WIDEN);
    SendMessageW(g_progress, PBM_SETRANGE32, 0, 1000);

    make(0, L"STATIC", L"결과", 0, MARGIN, 162, 200, 20, 0, g_font, 0);

    tip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP,
                          CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                          g_wnd, NULL, g_inst, NULL);

    for (i = 0, y = 188; i < 4; i++, y += 34) {
        make(0, L"STATIC", LABELS[i], SS_CENTERIMAGE, MARGIN, y, 70, 26, 0, g_font, 0);
        g_result[i] = make(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL,
                           MARGIN + 74, y, RESULT_W, 26, ID_RESULT0 + i, g_mono, WIDEN);
        g_copy[i] = make(0, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON | BS_ICON,
                         right - 36, y, 36, 26, ID_COPY0 + i, g_font, SHIFT);
        SendMessageW(g_copy[i], BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_copy_icon);

        memset(&ti, 0, sizeof ti);
        ti.cbSize = sizeof ti;
        ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        ti.hwnd = g_wnd;
        ti.uId = (UINT_PTR)g_copy[i];
        ti.lpszText = (LPWSTR)L"복사";
        SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    }

    clear_results();
    set_busy(0);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int id, i;
    switch (msg) {
    case WM_COMMAND:
        id = LOWORD(wp);
        if (id == ID_PATH && HIWORD(wp) == EN_CHANGE) {
            if (!g_busy) clear_results();         /* 파일이 바뀌면 이전 결과는 지움 */
            return 0;
        }
        if (HIWORD(wp) != BN_CLICKED) break;
        switch (id) {
        case ID_BROWSE: if (!g_busy) browse(); return 0;
        case ID_RUN:    start_job(); return 0;
        case ID_STOP:   if (g_busy) InterlockedExchange(&g_cancel, 1); return 0;
        case ID_RESET:
            if (!g_busy) { set_path(L""); clear_results(); SetFocus(g_path); }
            return 0;
        }
        if (id >= ID_COPY0 && id < ID_COPY0 + 4) { copy_result(id - ID_COPY0); return 0; }
        break;

    case WM_APP_PROGRESS:
        if (g_busy) SendMessageW(g_progress, PBM_SETPOS, wp, 0);
        return 0;

    case WM_APP_DONE:
        job_done(wp, lp);
        return 0;

    case WM_DROPFILES:
        on_drop((HDROP)wp);
        return 0;

    case WM_CTLCOLORSTATIC:
        /* 결과 칸(읽기 전용)은 흰 바탕으로 */
        for (i = 0; i < 4; i++) {
            if ((HWND)lp == g_result[i]) {
                SetBkColor((HDC)wp, GetSysColor(COLOR_WINDOW));
                SetTextColor((HDC)wp, GetSysColor(COLOR_WINDOWTEXT));
                return (LRESULT)g_white;
            }
        }
        break;

    case WM_CLOSE:
        InterlockedExchange(&g_cancel, 1);
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void make_fonts(void)
{
    NONCLIENTMETRICSW ncm;
    LOGFONTW lf;
    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0)) {
        lf = ncm.lfMessageFont;
    } else {
        memset(&lf, 0, sizeof lf);
        lf.lfHeight = -S(12);
        lf.lfCharSet = DEFAULT_CHARSET;
    }
    lf.lfHeight = -MulDiv(10, g_dpi, 72);           /* 10pt: 기본(9pt)보다 조금 크게 */
    g_font = CreateFontIndirectW(&lf);

    memset(&lf, 0, sizeof lf);                      /* 해시값: 고정폭 글꼴 */
    lf.lfHeight = -MulDiv(10, g_dpi, 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    lstrcpyW(lf.lfFaceName, L"Consolas");
    g_mono = CreateFontIndirectW(&lf);
}

/* CRT 없이 시작하는 진입점 (링크 옵션 /entry:entry, GUI 서브시스템) */
void __stdcall entry(void)
{
    WNDCLASSEXW wc;
    INITCOMMONCONTROLSEX icc;
    RECT rc, work;
    MSG m;
    HDC dc;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;    /* 닫기(X) 버튼만 */
    int w, h, argc = 0;
    wchar_t **argv;

    g_inst = GetModuleHandleW(NULL);
    crc32_init_table();

    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    if (g_dpi <= 0) g_dpi = 96;

    make_fonts();
    g_white = GetSysColorBrush(COLOR_WINDOW);
    g_copy_icon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_COPY), IMAGE_ICON,
                                    S(16), S(16), LR_DEFAULTCOLOR);

    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = g_inst;
    wc.hIcon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"hashsum-gui";
    RegisterClassExW(&wc);

    measure();
    rc.left = 0; rc.top = 0; rc.right = S(CLIENT_W) + g_extra; rc.bottom = S(CLIENT_H);
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_ACCEPTFILES);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    g_wnd = CreateWindowExW(WS_EX_ACCEPTFILES, L"hashsum-gui", APP_TITLE, style,
                            work.left + (work.right - work.left - w) / 2,
                            work.top + (work.bottom - work.top - h) / 2,
                            w, h, NULL, NULL, g_inst, NULL);
    create_controls();

    /* EXE 아이콘에 파일을 끌어다 놓아 실행한 경우: 경로만 채움 */
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc >= 2) set_path(argv[1]);
        LocalFree(argv);
    }

    ShowWindow(g_wnd, SW_SHOWNORMAL);
    UpdateWindow(g_wnd);
    SetFocus(argc >= 2 ? g_run : g_path);

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(g_wnd, &m)) continue;   /* Tab 이동, Enter=출력, Esc=취소 */
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    ExitProcess((UINT)m.wParam);
}
