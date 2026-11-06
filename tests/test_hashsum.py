#!/usr/bin/env python3
"""
hashsum 검증 테스트 - Windows 버전(hashsum.exe)과 셸 스크립트 버전(hashsum.sh) 공용

사용법:
    python tests/test_hashsum.py build/windows-x86/hashsum.exe
    python3 tests/test_hashsum.py unix/hashsum.sh
    python3 tests/test_hashsum.py unix/hashsum.sh --shell zsh
    python3 tests/test_hashsum.py build/windows-x86/hashsum.exe --runner wine

옵션:
    --shell SHELL    .sh 를 실행할 셸 (기본: sh)
    --runner CMD     실행 파일 앞에 붙일 명령 (예: Linux 에서 wine 으로 .exe 실행)

Python 표준 라이브러리(zlib, hashlib)로 계산한 값과 hashsum 의 출력을 비교한다.
테스트 파일은 임시 폴더에 만들고, 끝나면 지운다. 모두 통과하면 종료 코드 0.
"""
import argparse
import hashlib
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import zlib

VERSION = "1.6"
LABELS = ["CRC32", "MD5", "SHA-1", "SHA-256"]
ALL_KEYS = ["File", "Size"] + LABELS
OPT_NAME = {"CRC32": "crc32", "MD5": "md5", "SHA-1": "sha1", "SHA-256": "sha256"}


class Runner:
    def __init__(self, program, shell, runner, cwd):
        program = os.path.abspath(program)
        self.is_exe = program.lower().endswith(".exe")
        prefix = shlex.split(runner) if runner else []
        if self.is_exe:
            self.cmd = prefix + [program]
        else:
            self.cmd = prefix + [shell, program]
        self.cwd = cwd

    def __call__(self, *args):
        p = subprocess.run(self.cmd + list(args), cwd=self.cwd,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        out = p.stdout.decode("utf-8", "replace").replace("\r\n", "\n")
        err = p.stderr.decode("utf-8", "replace").replace("\r\n", "\n")
        return p.returncode, out, err


class Checker:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def __call__(self, name, cond, detail=""):
        if cond:
            self.passed += 1
            print("  [OK]   " + name)
        else:
            self.failed += 1
            print("  [FAIL] " + name)
            if detail:
                for line in detail.strip().splitlines()[:12]:
                    print("           | " + line)


def expected(data, group=False):
    v = {
        "CRC32": "%08x" % (zlib.crc32(data) & 0xFFFFFFFF),
        "MD5": hashlib.md5(data).hexdigest(),
        "SHA-1": hashlib.sha1(data).hexdigest(),
        "SHA-256": hashlib.sha256(data).hexdigest(),
    }
    if group:
        v = {k: " ".join(re.findall("....", x)) for k, x in v.items()}
    v["Size"] = "%d bytes" % len(data)
    return v


def parse(out):
    """출력 -> [{'File':..., 'Size':..., 'CRC32':..., ...}, ...] (블록 = 파일 하나)"""
    blocks, cur = [], []
    for line in out.split("\n"):
        if not line.strip():
            if cur:
                blocks.append(cur)
                cur = []
            continue
        if " : " in line:
            k, v = line.split(" : ", 1)
            cur.append((k.strip(), v))
    if cur:
        blocks.append(cur)
    return blocks


def keys(block):
    return [k for k, _ in block]


def as_dict(block):
    return dict(block)


def main():
    ap = argparse.ArgumentParser(description="hashsum 검증 테스트")
    ap.add_argument("program", help="hashsum.exe 또는 hashsum.sh 경로")
    ap.add_argument("--shell", default="sh", help=".sh 를 실행할 셸 (기본: sh)")
    ap.add_argument("--runner", default="", help="실행 파일 앞에 붙일 명령 (예: wine)")
    a = ap.parse_args()

    if not os.path.isfile(a.program):
        print("실행 파일이 없습니다: " + a.program)
        return 2

    tmp = tempfile.mkdtemp(prefix="hashsum_test_")
    run = Runner(a.program, a.shell, a.runner, tmp)
    t = Checker()
    win = run.is_exe
    prefixes = ["-", "/"] if win else ["-"]

    files = {
        "empty.bin": b"",
        "abc.txt": b"abc",
        "len55.bin": os.urandom(55),            # 패딩 경계 직전
        "len56.bin": os.urandom(56),            # 패딩이 다음 블록으로 넘어가는 경계
        "len64.bin": os.urandom(64),            # 정확히 한 블록
        "mb1.bin": os.urandom(1024 * 1024 + 1),       # 읽기 버퍼(1MB) 경계
        "big.bin": os.urandom(3 * 1024 * 1024 + 7),   # 버퍼 여러 번
        "한글 파일명.txt": "유니코드 파일명 테스트".encode("utf-8"),
        "a$b`c (1).txt": b"special characters",
    }
    for name, data in files.items():
        with open(os.path.join(tmp, name), "wb") as f:
            f.write(data)
    os.mkdir(os.path.join(tmp, "a folder"))
    with open(os.path.join(tmp, "-md5"), "wb") as f:      # 옵션처럼 생긴 파일명
        f.write(b"zz")
    with open(os.path.join(tmp, "-g4"), "wb") as f:       # 1.5 에서 없앤 옵션 이름
        f.write(b"g4")
    big = files["big.bin"]
    E = expected(big)
    G = expected(big, group=True)

    print("hashsum 테스트: %s (%s 버전)" % (" ".join(run.cmd), "Windows" if win else "셸 스크립트"))

    # ---------------------------------------------------------- 해시값
    print("\n[해시값]")
    for name, data in files.items():
        rc, out, err = run(name)
        b = parse(out)
        exp = expected(data)
        exp["File"] = name
        ok = (rc == 0 and len(b) == 1 and keys(b[0]) == ALL_KEYS and
              all(as_dict(b[0]).get(k) == v for k, v in exp.items()))
        t("%s (%d bytes)" % (name, len(data)), ok, out + err)

    # ---------------------------------------------------------- 4자리 끊기
    print("\n[-g : 4자리씩 끊어서 출력]")
    g_opts = ["-g", "--group"] + (["/g", "/G"] if win else [])
    for opt in g_opts:
        rc, out, err = run("big.bin", opt)            # 옵션이 파일 뒤에 와도 됨
        b = parse(out)
        ok = rc == 0 and b and all(as_dict(b[0]).get(k) == G[k] for k in LABELS)
        t(opt, ok, out + err)
    rc, out, err = run("-g", "abc.txt")
    b = parse(out)
    t("-g : CRC32 = '3524 41c2'", b and as_dict(b[0]).get("CRC32") == "3524 41c2", out)

    # ---------------------------------------------------------- 해시 선택
    print("\n[해시 선택 옵션]")
    for p in prefixes:
        for label in LABELS:
            opt = p + OPT_NAME[label]
            rc, out, err = run(opt, "big.bin")
            b = parse(out)
            ok = (rc == 0 and len(b) == 1 and keys(b[0]) == ["File", "Size", label] and
                  as_dict(b[0])[label] == E[label])
            t(opt, ok, out + err)
        rc, out, err = run(p + "sha256", p + "md5", "big.bin")   # 순서는 항상 고정
        b = parse(out)
        ok = rc == 0 and b and keys(b[0]) == ["File", "Size", "MD5", "SHA-256"]
        t("%ssha256 %smd5 -> MD5, SHA-256 순서" % (p, p), ok, out)
        rc, out, err = run(p + "sha1", p + "g", "big.bin")
        b = parse(out)
        ok = rc == 0 and b and keys(b[0]) == ["File", "Size", "SHA-1"] and as_dict(b[0])["SHA-1"] == G["SHA-1"]
        t("%ssha1 %sg" % (p, p), ok, out)
        args = [p + OPT_NAME[k] for k in LABELS] + ["big.bin"]
        rc, out, err = run(*args)
        b = parse(out)
        ok = rc == 0 and b and keys(b[0]) == ALL_KEYS and all(as_dict(b[0])[k] == E[k] for k in LABELS)
        t("네 개 모두 지정 = 기본 출력", ok, out)
    rc, out, err = run("-md5", "abc.txt", "empty.bin")
    b = parse(out)
    ok = (rc == 0 and len(b) == 2 and keys(b[0]) == keys(b[1]) == ["File", "Size", "MD5"] and
          as_dict(b[1])["MD5"] == hashlib.md5(b"").hexdigest())
    t("-md5 + 파일 2개", ok, out)
    if win:
        rc, out, err = run("/SHA256", "/G", "big.bin")
        b = parse(out)
        t("대소문자 무시: /SHA256 /G", b and keys(b[0]) == ["File", "Size", "SHA-256"] and
          as_dict(b[0])["SHA-256"] == G["SHA-256"], out)
        rc, out, err = run("-Md5", "big.bin")
        b = parse(out)
        t("대소문자 무시: -Md5", b and keys(b[0]) == ["File", "Size", "MD5"], out)

    # ---------------------------------------------------------- 버전 / 도움말
    print("\n[버전 / 도움말]")
    v_opts = ["-v", "--version"] + (["/v", "/V"] if win else [])
    for opt in v_opts:
        rc, out, err = run(opt)
        t(opt + " -> 'hashsum %s'" % VERSION, rc == 0 and out.strip() == "hashsum " + VERSION, out + err)
    rc, out, err = run("big.bin", "-v")
    t("-v 가 파일 뒤에 있어도 버전만 출력", rc == 0 and out.strip() == "hashsum " + VERSION, out)
    h_opts = ["-h", "--help"] + (["/?", "/h"] if win else [])
    for opt in h_opts:
        rc, out, err = run(opt)
        ok = rc == 0 and "사용법" in out and all(x in out for x in ["crc32", "md5", "sha1", "sha256", "-g", "-v"])
        t(opt + " -> 도움말", ok, out + err)
    rc, out, err = run()
    t("인자 없음 -> 도움말, 종료코드 1", rc == 1 and "사용법" in out, out + err)
    rc, out, err = run("-h")
    t("도움말에 없앤 옵션 -g4 가 없음", "g4" not in out, out)

    # ---------------------------------------------------------- 오류 처리
    print("\n[오류 처리]")
    rc, out, err = run("abc.txt", "no_such_file", "empty.bin")
    b = parse(out)
    t("없는 파일: 종료코드 1, 나머지 파일은 정상 출력",
      rc == 1 and len(b) == 2 and "no_such_file" in err, out + err)
    rc, out, err = run("a folder")
    t("폴더 지정: 종료코드 1 + 오류 메시지", rc == 1 and "폴더" in err, out + err)
    rc, out, err = run("-sha1", "--", "-md5")
    b = parse(out)
    t("'--' 뒤의 -md5 는 파일명", rc == 0 and b and as_dict(b[0]).get("File") == "-md5" and
      as_dict(b[0]).get("SHA-1") == hashlib.sha1(b"zz").hexdigest(), out + err)
    rc, out, err = run("-g4", "abc.txt")
    b = parse(out)
    t("-g4 는 옵션이 아님 (1.5 에서 제거) -> 파일명으로 처리",
      rc == 0 and len(b) == 2 and as_dict(b[0]).get("File") == "-g4" and
      as_dict(b[1]).get("CRC32") == "352441c2", out + err)
    rc, out, err = run("-sha265", "abc.txt")
    b = parse(out)
    t("옵션 오타(-sha265): 오류, 나머지 파일은 처리", rc == 1 and len(b) == 1 and "-sha265" in err, out + err)
    if not win:
        rc, out, err = run("/md5")
        t("셸 버전: /md5 는 경로로 처리", rc == 1 and "/md5" in err, out + err)

    shutil.rmtree(tmp, ignore_errors=True)
    print("\n결과: %d 통과, %d 실패" % (t.passed, t.failed))
    return 0 if t.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
