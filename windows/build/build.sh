#!/bin/sh
# ----------------------------------------------------------------------
# hashsum.exe 크로스 빌드 스크립트 (Linux / macOS 에서 Windows EXE 생성)
#
# 사용법 : ./build.sh [x86 | x64 | all]      (기본: x86)
#
#   x86      32비트 EXE - Windows 7 이상의 32/64비트 Windows 모두에서 실행
#   x64      64비트 EXE
#   all      둘 다
#
# 결과물 : windows/bin/windows-<arch>/hashsum.exe
#
# 필요한 도구 : Zig (https://ziglang.org)
#   - 압축을 풀어 PATH 에 추가하거나 패키지 관리자로 설치 (brew install zig 등)
#   - pip 로 설치한 경우 : ZIG="python3 -m ziglang" ./build.sh
#
# C 런타임(CRT) 없이 링크하므로 결과물은 KERNEL32.dll, SHELL32.dll 만 사용합니다.
# ----------------------------------------------------------------------
set -e

ZIG=${ZIG:-zig}
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=$(cd "$HERE/.." && pwd)/bin
SRC="$HERE/hashsum.c"

CFLAGS="-Os -Wall -Wextra -fno-builtin -fno-stack-protector"
# 진입점 entry, 콘솔 프로그램, 서브시스템 버전 6.1 (= Windows 7)
# /Brepro : 빌드 시각을 기록하지 않음 (같은 소스면 같은 EXE)
LDFLAGS="-nostdlib -s -Wl,--entry=entry -Wl,--subsystem,console \
-Wl,--major-subsystem-version,6 -Wl,--minor-subsystem-version,1 -Wl,/Brepro -lkernel32 -lshell32"

build() {
    arch=$1
    case $arch in
        x86) target=x86-windows.win7-gnu ;;
        x64) target=x86_64-windows.win7-gnu ;;
    esac
    out="$BIN/windows-$arch"
    mkdir -p "$out"
    echo "[zig] $target -> windows/bin/windows-$arch/hashsum.exe"
    # 컴파일과 링크를 나눈다: 한 번에 하면 MinGW 시작 코드(UCRT 의존)가 붙는다
    $ZIG cc -target "$target" $CFLAGS -c "$SRC" -o "$out/hashsum.o"
    $ZIG cc -target "$target" "$out/hashsum.o" $LDFLAGS -o "$out/hashsum.exe"
    rm -f "$out/hashsum.o" "$out/hashsum.pdb"
}

if ! $ZIG version >/dev/null 2>&1; then
    echo "Zig 를 찾을 수 없습니다: $ZIG" >&2
    echo "  설치: https://ziglang.org/download/  또는  pip install ziglang" >&2
    echo "  pip 로 설치했다면: ZIG=\"python3 -m ziglang\" $0 $*" >&2
    exit 1
fi

case ${1:-x86} in
    x86) build x86 ;;
    x64) build x64 ;;
    all) build x86; build x64 ;;
    *)   echo "사용법: $0 [x86 | x64 | all]" >&2; exit 2 ;;
esac
