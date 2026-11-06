# hashsum

파일의 **CRC32 / MD5 / SHA-1 / SHA-256** 해시값을 한 번에 보여 주는 도구입니다.

- **Windows GUI 버전** `hashsum-gui.exe` — 창에서 파일을 골라 해시값을 확인하고 복사합니다.
- **Windows 명령줄 버전** `hashsum.exe` — 터미널에서 실행합니다.
- **Linux / macOS 버전** `hashsum.sh` — 운영체제에 기본으로 들어 있는 명령만 쓰는 셸 스크립트입니다.

Windows 버전은 둘 다 Windows 7 이상(32/64비트)에서 설치나 런타임 없이 실행됩니다.
명령줄 버전과 셸 스크립트 버전은 출력 형식과 옵션이 같습니다.

```
> hashsum.exe setup.zip
File    : setup.zip
Size    : 3 bytes
CRC32   : 352441c2
MD5     : 900150983cd24fb0d6963f7d28e17f72
SHA-1   : a9993e364706816aba3e25717850c26c9cd0d89d
SHA-256 : ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad

> hashsum.exe -sha256 -g setup.zip
File    : setup.zip
Size    : 3 bytes
SHA-256 : ba78 16bf 8f01 cfea 4141 40de 5dae 2223 b003 61a3 9617 7a9c b410 ff61 f200 15ad
```

## GUI 버전 사용법 (`hashsum-gui.exe`)

`hashsum-gui.exe` 를 더블클릭해서 실행합니다.

```
┌──────────────────────────────────────────────────────┐
│ 선택 파일                                              │
│ [C:\Downloads\setup.zip                    ] [파일선택] │
│ [            출력            ] [ 취소 ] [ 초기화 ]       │
│ [■■■■■■■■■■■■■■■■■ 진행률 ■■■■■■■■■■■■■■■■■■■■■■■■] │
│ 결과                                                   │
│ CRC32    [352441c2                             ] [⧉]   │
│ MD5      [900150983cd24fb0d6963f7d28e17f72     ] [⧉]   │
│ SHA-1    [a9993e364706816aba3e25717850c26c9cd0d89d] [⧉] │
│ SHA-256  [ba7816bf8f01cfea414140de5dae2223b0...] [⧉]   │
└──────────────────────────────────────────────────────┘
```

1. **파일 지정**: **파일선택** 버튼으로 고르거나, 탐색기에서 파일을 창에 끌어다 놓습니다.
   경로를 직접 입력하거나 붙여 넣어도 됩니다 (앞뒤 따옴표는 자동으로 뺍니다).
   탐색기에서 파일을 `hashsum-gui.exe` 아이콘 위에 끌어다 놓아 실행하면 그 파일이 지정된 채로 열립니다.
2. **출력** 버튼(또는 Enter)을 누르면 계산해서 아래 네 칸에 결과를 표시합니다.
   큰 파일은 진행률 막대로 진행 상황을 보여 주고, **취소** 버튼(또는 Esc)으로 중단할 수 있습니다.
3. **복사**: 결과값을 마우스로 선택해 Ctrl+C 로 복사하거나, 오른쪽의 복사 버튼을 누릅니다.
4. **초기화** 버튼은 선택한 파일과 결과를 모두 지웁니다.

- 파일을 바꾸면 이전 결과는 자동으로 지워집니다.
- 폴더를 지정했거나 파일을 읽을 수 없으면 메시지 상자로 알려 줍니다.
- 고해상도 화면(화면 배율 125%, 150% 등)에 맞춰 크기가 조정됩니다.

## 명령줄 버전 사용법

```
hashsum.exe [옵션] <파일> [파일2 ...]       (Windows)
./hashsum.sh [옵션] <파일> [파일2 ...]      (Linux / macOS)
```

| 옵션 | 설명 |
|---|---|
| `-crc32` | CRC32 해시값만 출력 |
| `-md5` | MD5 해시값만 출력 |
| `-sha1` | SHA-1 해시값만 출력 |
| `-sha256` | SHA-256 해시값만 출력 |
| `-g`, `--group` | 해시값을 4자리씩 끊어서 출력 (예: `3524 41c2`) |
| `-v`, `--version` | 버전 표시 |
| `-h`, `--help` | 도움말 출력 (Windows 는 `/?` 도 가능) |
| `--` | 이후 인자는 모두 파일명으로 처리 (`-md5` 라는 이름의 파일 등) |

- 해시 선택 옵션은 여러 개를 함께 쓸 수 있습니다. 하나도 지정하지 않으면 네 개를 모두 출력합니다.
  출력 순서는 옵션 순서와 상관없이 항상 CRC32 → MD5 → SHA-1 → SHA-256 입니다.
- 선택하지 않은 해시는 계산하지 않으므로 그만큼 빨라집니다.
- 옵션은 파일명 앞뒤 어디에 와도 됩니다.
- **Windows 버전**은 `/` 형식(`/sha256`, `/g`, `/v`)도 받고, 옵션의 대소문자를 구분하지 않습니다.
- **셸 스크립트 버전**은 `-` 형식만 받습니다. Linux / macOS 에서 `/md5` 는 경로이기 때문입니다.
- 종료 코드: 모든 파일을 처리하면 `0`, 하나라도 실패하거나 파일을 지정하지 않으면 `1`.
- 한글 등 유니코드 파일명과 4GB 를 넘는 파일을 지원합니다.

## 설치

### Windows

빌드된 실행 파일이 저장소의 `windows/bin/` 폴더에 들어 있습니다. 원하는 폴더에 복사해서 실행하면 됩니다.
PATH 에 포함된 폴더(예: 직접 만든 도구 폴더)에 두면 어디서든 실행할 수 있습니다.

| 파일 | 용도 |
|---|---|
| `windows/bin/windows-x86/hashsum-gui.exe` | GUI, 32비트 (권장). 32비트와 64비트 Windows 에서 모두 실행 |
| `windows/bin/windows-x86/hashsum.exe` | 명령줄, 32비트 (권장) |
| `windows/bin/windows-x64/hashsum-gui.exe` | GUI, 64비트. 64비트 Windows 전용 |
| `windows/bin/windows-x64/hashsum.exe` | 명령줄, 64비트. 64비트 Windows 전용 |

- C 런타임(CRT) 없이 빌드되어 Windows 에 기본으로 들어 있는 DLL 만 사용합니다.
  VC++ 재배포 패키지가 설치되지 않은 Windows 7 에서도 실행됩니다.
- 서명되지 않은 실행 파일이라 처음 실행할 때 SmartScreen 경고가 나올 수 있습니다.

### Linux / macOS

```sh
chmod +x unix/hashsum.sh
./unix/hashsum.sh 파일
# 어디서든 쓰려면
sudo cp unix/hashsum.sh /usr/local/bin/hashsum
```

셸 스크립트가 사용하는 명령은 다음과 같습니다. 설치된 것을 자동으로 골라 씁니다.

| 해시 | Linux | macOS | 둘 다 없을 때 |
|---|---|---|---|
| MD5 | `md5sum` | `md5 -q` | `openssl` |
| SHA-1 | `sha1sum` | `shasum -a 1` | `openssl` |
| SHA-256 | `sha256sum` | `shasum -a 256` | `openssl` |
| CRC32 | `gzip` | `gzip` | — |

CRC32 는 두 OS 에 공통으로 있는 전용 명령이 없어서, gzip 이 압축 파일 끝에 기록하는 원본의
CRC32 값을 읽어 씁니다. 이 값은 ZIP·PNG 에서 쓰는 표준 CRC32 와 같습니다.
(`cksum` 의 기본 CRC 는 다른 알고리즘이라 값이 다릅니다.)

셸 스크립트는 해시마다 파일을 따로 읽고 CRC32 는 gzip 압축을 거치기 때문에,
수 GB 짜리 파일에서는 Windows 버전보다 느립니다. 필요한 해시만 골라 쓰면 빨라집니다.

> `source hashsum.sh` 처럼 현재 셸에 불러와 실행하지 마세요. 끝날 때 `exit` 를 호출하므로 터미널 창이 닫힙니다.

## 빌드 (Windows 버전)

### Windows 에서: `build.bat`

```bat
windows\build\build.bat            :: 32비트 -> windows\bin\windows-x86\
windows\build\build.bat x64        :: 64비트 -> windows\bin\windows-x64\
windows\build\build.bat x86 msvc   :: 컴파일러 지정 (zig | msvc | mingw)
```

명령줄 버전(`hashsum.exe`)과 GUI 버전(`hashsum-gui.exe`)을 함께 빌드합니다.

컴파일러를 지정하지 않으면 Zig → MSVC → MinGW 순서로 찾습니다.

| 컴파일러 | 준비 |
|---|---|
| [Zig](https://ziglang.org/download/) (권장) | 압축을 풀고 PATH 에 추가. 또는 `pip install ziglang` 후 `set ZIG=python -m ziglang` |
| MSVC | Visual Studio Build Tools 설치 후 **x86 / x64 Native Tools Command Prompt** 에서 실행 |
| MinGW-w64 | [MSYS2](https://www.msys2.org/) 등에서 설치. 32비트는 `i686`, 64비트는 `x86_64` 용 gcc |

### Linux / macOS 에서: `build.sh` (크로스 빌드)

```sh
./windows/build/build.sh           # 32비트 -> windows/bin/windows-x86/
./windows/build/build.sh x64       # 64비트 -> windows/bin/windows-x64/
./windows/build/build.sh all       # 둘 다
ZIG="python3 -m ziglang" ./windows/build/build.sh all    # Zig 를 pip 로 설치한 경우
```

[Zig](https://ziglang.org) 만 있으면 됩니다 (`brew install zig`, 또는 `pip install ziglang`).

두 스크립트 모두 빌드 결과를 `windows/bin/` 에 바로 만들므로, 빌드하면 배포용 실행 파일이 갱신됩니다.
저장소에 들어 있는 실행 파일은 Zig 0.16.0 으로 빌드했습니다.

### C 런타임 없이 빌드하는 이유와 주의점

Windows 7 기본 설치에는 최신 C 런타임(UCRT)이 없습니다. 그래서 두 소스 모두 진입점(`entry`)과
`memcpy` / `memset` 을 직접 제공하고, 링크할 때 C 런타임을 빼서 Windows 기본 DLL 만 쓰게 했습니다.
소스를 고칠 때는 다음을 지켜야 C 런타임 함수가 끼어들지 않습니다.

- 표준 C 라이브러리 함수(`printf`, `malloc`, `strlen` 등)를 쓰지 않는다.
- 64비트 정수의 나눗셈이나 변수만큼의 시프트를 쓰지 않는다 (32비트 빌드에서 보조 함수가 필요해짐).
- 4KB 가 넘는 지역 변수를 두지 않는다 (`__chkstk` 호출이 생김).

GUI 버전의 해시 계산 코드는 명령줄 버전(`hashsum.c`)과 같습니다. 한쪽을 고치면 다른 쪽도 함께 고쳐야 합니다.

## 폴더 구성

```
windows/
  bin/
    windows-x86/   배포용 32비트 EXE (hashsum.exe, hashsum-gui.exe)
    windows-x64/   배포용 64비트 EXE (hashsum.exe, hashsum-gui.exe)
  build/
    hashsum.c              명령줄 버전 소스 (단일 파일)
    hashsum-gui.c          GUI 버전 소스 (단일 파일)
    hashsum-gui.rc         GUI 리소스: 아이콘, manifest, 버전 정보
    hashsum-gui.manifest   화면 모양(비주얼 스타일), 고해상도 화면 대응, 지원 OS
    hashsum-gui.ico        프로그램 아이콘
    copy.ico               복사 버튼 아이콘
    build.bat              Windows 에서 빌드 (Zig / MSVC / MinGW)
    build.sh               Linux / macOS 에서 크로스 빌드 (Zig)
unix/
  hashsum.sh       Linux / macOS 버전 (POSIX sh)
CHANGELOG.md       변경 이력
```

## 변경 이력

[CHANGELOG.md](CHANGELOG.md) 를 참고하세요.
