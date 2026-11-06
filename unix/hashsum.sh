#!/bin/sh
# ----------------------------------------------------------------------
# hashsum.sh - 파일의 CRC32 / MD5 / SHA-1 / SHA-256 해시값 출력 (Linux / macOS)
#
# 사용법 : ./hashsum.sh [옵션] <파일> [파일2 ...]   (옵션은 ./hashsum.sh -h 참고)
#
# 운영체제에 기본으로 들어 있는 명령만 사용합니다. (별도 설치 불필요)
#   MD5     : md5sum (Linux)      | md5 (macOS)           | openssl
#   SHA-1   : sha1sum (Linux)     | shasum -a 1 (macOS)   | openssl
#   SHA-256 : sha256sum (Linux)   | shasum -a 256 (macOS) | openssl
#   CRC32   : gzip  - gzip 형식 끝에 기록되는 원본의 CRC32 (ZIP/PNG 와 같은 표준 CRC32)
#             ※ cksum 의 기본 CRC 는 다른 알고리즘이라 값이 다릅니다.
#
# POSIX sh 문법만 사용 (Linux dash/bash, macOS sh/bash/zsh 에서 동작)
# ----------------------------------------------------------------------

VERSION="1.6"
PROG=$(basename "$0")

usage() {
    cat <<EOF
hashsum $VERSION - 파일의 CRC32 / MD5 / SHA-1 / SHA-256 해시값 출력

사용법: $PROG [옵션] <파일> [파일2 ...]

옵션  :
  -crc32      CRC32 해시값만 출력
  -md5        MD5 해시값만 출력
  -sha1       SHA-1 해시값만 출력
  -sha256     SHA-256 해시값만 출력
              (여러 개를 함께 지정 가능, 지정하지 않으면 전부 출력)
  -g          해시값을 4자리씩 끊어서 출력 (예: 3524 41c2)
  -v          버전 표시
  -h, --help  도움말 출력
  --          이후 인자는 모두 파일명으로 처리

예시  : $PROG setup.zip
        $PROG -sha256 setup.zip
        $PROG -md5 -sha1 -g setup.zip
        $PROG "My Files/문서.zip" data.bin
EOF
}

err() { printf '오류: %s\n' "$*" >&2; }

# ------------------------------------------------------ 옵션 판별
# 인자가 옵션이면 종류를 출력: end / help / version / group / crc32 / md5 / sha1 / sha256
opt_kind() {
    case $1 in
        --)                     echo end ;;
        -h|--help)              echo help ;;
        -v|--version)           echo version ;;
        -g|--group)             echo group ;;
        -crc32)                 echo crc32 ;;
        -md5)                   echo md5 ;;
        -sha1)                  echo sha1 ;;
        -sha256)                echo sha256 ;;
        *)                      echo none ;;
    esac
}

# --------------------------------------------- 1단계: 옵션 해석
# 옵션은 파일명 앞뒤 어디에 와도 됨
GROUP=0
SEL_CRC32=0; SEL_MD5=0; SEL_SHA1=0; SEL_SHA256=0
nfiles=0
opts_done=0
for a in "$@"; do
    kind=none
    [ "$opts_done" -eq 0 ] && kind=$(opt_kind "$a")
    case $kind in
        end)     opts_done=1 ;;
        help)    usage; exit 0 ;;
        version) echo "hashsum $VERSION"; exit 0 ;;
        group)   GROUP=1 ;;
        crc32)   SEL_CRC32=1 ;;
        md5)     SEL_MD5=1 ;;
        sha1)    SEL_SHA1=1 ;;
        sha256)  SEL_SHA256=1 ;;
        *)       nfiles=$((nfiles + 1)) ;;
    esac
done

if [ "$nfiles" -eq 0 ]; then
    usage
    exit 1
fi

# 해시 선택 옵션이 하나도 없으면 전부 출력
if [ $((SEL_CRC32 + SEL_MD5 + SEL_SHA1 + SEL_SHA256)) -eq 0 ]; then
    SEL_CRC32=1; SEL_MD5=1; SEL_SHA1=1; SEL_SHA256=1
fi

# ------------------------------------------------- 사용할 명령 결정
# 선택된 해시에 필요한 명령만 확인한다.
# 파일은 항상 표준입력(<)으로 넘긴다: 파일명에 공백·특수문자가 있어도 출력 형식이 바뀌지 않음
have() { command -v "$1" >/dev/null 2>&1; }

if [ "$SEL_MD5" -eq 1 ]; then
    if have md5sum; then
        md5_of() { md5sum < "$1" | awk '{print $1}'; }
    elif have md5; then
        md5_of() { md5 -q < "$1"; }
    elif have openssl; then
        md5_of() { openssl dgst -md5 < "$1" | awk '{print $NF}'; }
    else
        err "MD5 를 계산할 명령(md5sum, md5, openssl)을 찾을 수 없습니다."; exit 2
    fi
fi

if [ "$SEL_SHA1" -eq 1 ]; then
    if have sha1sum; then
        sha1_of() { sha1sum < "$1" | awk '{print $1}'; }
    elif have shasum; then
        sha1_of() { shasum -a 1 < "$1" | awk '{print $1}'; }
    elif have openssl; then
        sha1_of() { openssl dgst -sha1 < "$1" | awk '{print $NF}'; }
    else
        err "SHA-1 을 계산할 명령(sha1sum, shasum, openssl)을 찾을 수 없습니다."; exit 2
    fi
fi

if [ "$SEL_SHA256" -eq 1 ]; then
    if have sha256sum; then
        sha256_of() { sha256sum < "$1" | awk '{print $1}'; }
    elif have shasum; then
        sha256_of() { shasum -a 256 < "$1" | awk '{print $1}'; }
    elif have openssl; then
        sha256_of() { openssl dgst -sha256 < "$1" | awk '{print $NF}'; }
    else
        err "SHA-256 을 계산할 명령(sha256sum, shasum, openssl)을 찾을 수 없습니다."; exit 2
    fi
fi

if [ "$SEL_CRC32" -eq 1 ] && ! have gzip; then
    err "CRC32 계산에 필요한 gzip 을 찾을 수 없습니다."; exit 2
fi

# gzip 출력의 마지막 8바이트 = CRC32(4바이트, 리틀엔디언) + 크기
# 압축률은 상관없으므로 가장 빠른 -1 사용
crc32_of() {
    gzip -1 -c < "$1" | tail -c 8 | od -An -tx1 -N4 |
        awk '{ printf "%s%s%s%s\n", $4, $3, $2, $1 }'
}

size_of() { wc -c < "$1" | tr -d ' \t'; }

# -g 옵션: 4자리마다 공백
fmt() {
    if [ "$GROUP" -eq 1 ]; then
        printf '%s\n' "$1" | sed 's/..../& /g; s/ $//'
    else
        printf '%s\n' "$1"
    fi
}

# --------------------------------------------------------- 파일 처리
hash_file() {
    f=$1
    if [ -d "$f" ]; then
        err "$f : 폴더입니다. 파일을 지정해 주세요."; return 1
    fi
    if [ ! -e "$f" ]; then
        err "$f : 파일이 없습니다."; return 1
    fi
    if [ ! -r "$f" ]; then
        err "$f : 읽기 권한이 없습니다."; return 1
    fi

    size=$(size_of "$f") || { err "$f : 파일을 읽을 수 없습니다."; return 1; }
    if [ "$SEL_CRC32" -eq 1 ];  then crc=$(crc32_of "$f")  || { err "$f : CRC32 계산 실패"; return 1; }; fi
    if [ "$SEL_MD5" -eq 1 ];    then md5=$(md5_of "$f")    || { err "$f : MD5 계산 실패"; return 1; }; fi
    if [ "$SEL_SHA1" -eq 1 ];   then sha1=$(sha1_of "$f")  || { err "$f : SHA-1 계산 실패"; return 1; }; fi
    if [ "$SEL_SHA256" -eq 1 ]; then sha=$(sha256_of "$f") || { err "$f : SHA-256 계산 실패"; return 1; }; fi

    printf 'File    : %s\n' "$f"
    printf 'Size    : %s bytes\n' "$size"
    [ "$SEL_CRC32" -eq 1 ]  && printf 'CRC32   : %s\n' "$(fmt "$crc")"
    [ "$SEL_MD5" -eq 1 ]    && printf 'MD5     : %s\n' "$(fmt "$md5")"
    [ "$SEL_SHA1" -eq 1 ]   && printf 'SHA-1   : %s\n' "$(fmt "$sha1")"
    [ "$SEL_SHA256" -eq 1 ] && printf 'SHA-256 : %s\n' "$(fmt "$sha")"
    return 0
}

# --------------------------------------------- 2단계: 파일 처리
fail=0
count=0
opts_done=0
for a in "$@"; do
    if [ "$opts_done" -eq 0 ]; then
        kind=$(opt_kind "$a")
        [ "$kind" = end ] && { opts_done=1; continue; }
        [ "$kind" != none ] && continue
    fi
    [ "$count" -gt 0 ] && echo
    count=$((count + 1))
    hash_file "$a" || fail=1
done

exit "$fail"
