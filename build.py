#!/usr/bin/env python3
"""Build driver for mega-irc: Python, so that Windows, Linux and macOS
are all first-class development hosts.

    python3 build.py            the bank (build/bank/crypto.bin, chain.bin), the client and bin/IRC.D81
    python3 build.py bank       the bank alone, with its map in build/bank/crypto.map
    python3 build.py test       the host suites: the IRC line protocol, the clock, P-256, the certificate chain
    python3 build.py hostclient IRC over TLS on the host, build/host/tls_irc_host
    python3 build.py clean

It needs llvm-mos (mos-mega65-clang), CMake, c1541 from VICE, and the two
sibling checkouts ../mega-net and ../mega65-libc; it builds mega65-libc
and mega-net's image itself when they are missing. The test needs a host
C compiler (cc, clang or gcc).

Overrides: LLVM_MOS_DIR, MEGANET (path to the mega-net checkout),
LIBC_SRC, C1541, CC.
"""
import os, platform, re, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
BIN = ROOT / "bin"
IS_WINDOWS = platform.system() == "Windows"
MEGANET = Path(os.environ.get("MEGANET", ROOT.parent / "mega-net")).resolve()
LIBC_SRC = Path(os.environ.get("LIBC_SRC", ROOT.parent / "mega65-libc")).resolve()
LIBC_BUILD = BUILD / "libc"
SRC = ROOT / "src"
CRYPTO = SRC / "crypto"
TLS = SRC / "tls"
BANK = SRC / "bank"
# TLS_P256: P-256 key agreement in the engine, for a server that will not
# take x25519 (OFTC, REQUIREMENTS.md 5.12). The shared files compile it
# only under this switch, so gemini's builds are untouched.
HOST_CFLAGS = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-DTLS_P256"]
WARN = ["-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter"]


def die(msg):
    print(f"error: {msg}", file=sys.stderr); sys.exit(1)


def run(cmd, **kw):
    print("  " + " ".join(Path(c).name if os.sep in str(c) else str(c) for c in cmd))
    if subprocess.run([str(c) for c in cmd], **kw).returncode != 0:
        die("command failed")


def find_tool(exe, roots=(), env=None):
    if env and os.environ.get(env):
        return os.environ[env]
    name = exe + (".exe" if IS_WINDOWS else "")
    for r in roots:
        cand = Path(r) / "bin" / name
        if cand.is_file():
            return str(cand)
    found = shutil.which(name)
    if found:
        return found
    for cand in ("/opt/homebrew/bin/" + name, "/usr/local/bin/" + name):
        if Path(cand).is_file():
            return cand
    die(f"{exe} not found")


def mos_clang():
    roots = [os.environ["LLVM_MOS_DIR"]] if os.environ.get("LLVM_MOS_DIR") else []
    roots += [Path.home() / "llvm-mos", "/opt/llvm-mos", "/usr/local/llvm-mos"]
    return find_tool("mos-mega65-clang", roots)


def check_rmw(elf):
    """ssh REQUIREMENTS.md 5.6: the compiler once decremented the wrong
    zero-page word in a loop; tools/orphan_rmw.py finds that shape in a
    linked ELF."""
    if not elf.exists():
        return
    objdump = find_tool("llvm-objdump", [Path(mos_clang()).parent.parent])
    r = subprocess.run([sys.executable, str(ROOT / "tools" / "orphan_rmw.py"), str(elf), objdump])
    if r.returncode:
        die(f"{elf.name}: the loop miscompile of ssh 5.6 is back; rewrite the loop it names")


def check_headroom(mapfile):
    """The soft stack grows down from $D000 into whatever the program leaves
    above its .noinit, and the linker never reserves any: gemini's client
    once ended its data at $D059 and booted into a crash with no message
    (gemini 5.6). At least 1 KB is required here."""
    end = 0
    for line in mapfile.read_text().splitlines():
        m = re.match(r"^\s*([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\d+\s+\.(bss|noinit|data)$", line)
        if m and int(m.group(1), 16) < 0xD000:       # the ram region's sections; .bss.hi sits above the I/O
            end = max(end, int(m.group(1), 16) + int(m.group(2), 16))
    room = 0xD000 - end
    print(f"  data ends at ${end:04x}: {room} bytes for the soft stack")
    if room < 1024:
        die("less than 1 KB between the program's data and $D000: the boot will crash (gemini 5.6); find the bytes")


def ensure_libc():
    lib = LIBC_BUILD / "src" / "libmega65libc.a"
    if lib.is_file():
        return lib
    if not LIBC_SRC.is_dir():
        die(f"mega65-libc not found at {LIBC_SRC}; set LIBC_SRC")
    print("building mega65-libc for llvm-mos:")
    prefix = Path(mos_clang()).parent.parent
    run(["cmake", f"-DCMAKE_PREFIX_PATH={prefix}", "-B", LIBC_BUILD, "-S", LIBC_SRC])
    run(["cmake", "--build", LIBC_BUILD])
    return lib


def ensure_meganet():
    image = MEGANET / "build" / "m65" / "meganet.bin"
    tramp = MEGANET / "build" / "gen" / "meganet_tramp.c"
    if not (image.is_file() and tramp.is_file()):
        if not MEGANET.is_dir():
            die(f"mega-net not found at {MEGANET}; set MEGANET")
        print("building mega-net:")
        run([sys.executable, "build.py", "abi"], cwd=MEGANET)
    return image, tramp


# ---- the bank ------------------------------------------------------------

def crypto_sources():
    return sorted(CRYPTO.glob("*.c"))


def bank_sources():
    """The crypto the bank carries: chacha20_small.c in place of the SSH
    client's unrolled chacha20.c (gemini 5.4); sha512.c stays only because
    curve25519.c holds Ed25519 too, and the linker drops what nothing
    calls. pkcs1.c is among them: the chain's arithmetic (5.6)."""
    return [p for p in crypto_sources() if p.name != "chacha20.c"] + \
           [TLS / "keys.c", TLS / "x509.c", CRYPTO / "mulacc_m65.S", CRYPTO / "mp32_m65.S", CRYPTO / "mp256_m65.S"]


def emit_payload(gen, tramp, image, chain, high_size=0):
    """build/gen/ck_payload.{c,h}: the trampoline as a C array, and the
    images' sizes, for the client's boot code. The client's own HIGH image
    is linked after this is written, so its size goes in by a second
    pass (build_client); the client's code never sees the first value."""
    gen.mkdir(parents=True, exist_ok=True)
    tb = tramp.read_bytes()
    (gen / "ck_payload.h").write_text(
        "/* generated by build.py: the TLS bank's trampoline and the images' sizes */\n"
        "#ifndef CK_PAYLOAD_H\n#define CK_PAYLOAD_H\n"
        f"#define CK_TRAMP_SIZE {len(tb)}\n#define CK_BIN_SIZE {image.stat().st_size}UL\n"
        f"#define CK_CHAIN_SIZE {chain.stat().st_size}UL\n#define CK_HIGH_SIZE {high_size}UL\n"
        "extern const unsigned char ck_tramp_bin[CK_TRAMP_SIZE];\n#endif\n")
    body = ", ".join(f"0x{x:02x}" for x in tb)
    (gen / "ck_payload.c").write_text(f'#include "ck_payload.h"\nconst unsigned char ck_tramp_bin[CK_TRAMP_SIZE] = {{ {body} }};\n')


def build_bank():
    """The TLS bank: a headerless image for bank 1 (CRYPTO), its top
    window (CHAIN) and its trampoline. gemini's build_bank with this
    client's window objects (5.15)."""
    clang = mos_clang()
    out = BUILD / "bank"; out.mkdir(parents=True, exist_ok=True)
    gen = BUILD / "gen"
    defs = ["-DX509_NO_ED25519", "-DTLS_P256"]        # Ed25519 and its SHA-512: 15 KB no server needs (gemini)
    tramp = out / "ck_tramp.bin"
    print("bank trampoline:")
    run([clang, "-nostartfiles", "-nostdlib", "-T", BANK / "trampoline.ld", BANK / "trampoline.S", "-o", tramp])
    hi_objs = []
    for src in (TLS / "der.c", TLS / "chain.c", TLS / "roots.c", BANK / "chain_api.c"):   # named in crypto.ld: compiled apart, without LTO
        obj = out / (src.stem + ".o")
        run([clang, "-std=c99", "-Oz", "-fno-lto", "-c", "-DCHAIN_BANK", "-I", TLS, "-I", CRYPTO, "-I", BANK, "-I", SRC] + WARN + defs + [src, "-o", obj])
        hi_objs.append(obj)
    both = out / "bank.bin"
    print("bank image:")
    run([clang, "-std=c99", "-Oz", "-nostartfiles", "-T", BANK / "crypto.ld", f"-Wl,-Map={out / 'crypto.map'}",
         "-I", CRYPTO, "-I", TLS, "-I", BANK, "-I", SRC] + WARN + defs +
        [BANK / "jumptable.S", BANK / "api.c", BANK / "dma.c"] + hi_objs + bank_sources() + ["-o", both])
    data = both.read_bytes()
    RAM_LEN = 0x9E00
    if len(data) < RAM_LEN:
        die("the bank image is shorter than its first region; is OUTPUT_FORMAT FULL(ram)?")
    image = out / "crypto.bin"
    image.write_bytes(data[:RAM_LEN].rstrip(b"\0") or b"\0")
    chain = out / "chain.bin"
    chain.write_bytes(data[RAM_LEN:].rstrip(b"\0") or b"\0")
    first = image.read_bytes()[0]
    print(f"  {image.name}: {image.stat().st_size} bytes, first byte ${first:02x} ({'jmp: ok' if first == 0x4c else 'NOT a jmp!'})")
    print(f"  {chain.name}: {chain.stat().st_size} bytes (the chain check, for $1E000)")
    if first != 0x4c:
        die("the jump table is not at the start of the image")
    if chain.stat().st_size < 1000:
        die("the top window is nearly empty: crypto.ld's patterns matched nothing (gemini 5.8)")
    check_rmw(both.with_suffix(".bin.elf"))
    emit_payload(gen, tramp, image, chain)
    return image, chain


# ---- the client ----------------------------------------------------------

def cflags():
    return ["-Oz", "-I", str(LIBC_SRC / "include"), "-I", str(MEGANET / "src" / "abi"),
            "-I", str(MEGANET / "build" / "gen"), "-I", str(BUILD / "gen"),
            "-I", str(SRC / "platform"), "-I", str(SRC), "-I", str(TLS)] + WARN + ["-DTLS_P256",
            # the disk layer's two buffers in low RAM the ROM's reset rebuilds on exit (lowram.h, step 3)
            "-DF011_BUF_AT=0x1100", "-DBAM2_AT=0x1300"]


HIGH_OBJS = ("der.c", "policy.c", "log.c", "marks.c")   # named in src/m65/irc.ld: the window under the KERNAL, compiled apart (5.17)
RAM_LEN = 2 + 0xAFFF                                    # the PRG's header and its whole region; the HIGH image follows


def link_client(clang, lib, tramp, srcs, hi_objs, prg, high):
    """One link of the client: the PRG and, from the same file, the HIGH
    image for $E000, the objects irc.ld names placed there."""
    out = BIN / "irc.bin"
    run([clang] + cflags() + ["-T", str(SRC / "m65" / "irc.ld")] + srcs + hi_objs +
        [str(BUILD / "gen" / "ck_payload.c"), str(tramp), str(MEGANET / "src" / "abi" / "meganet_vectors.c"), str(lib),
         f"-Wl,-Map={BIN / 'irc.map'}", "-o", str(out)])
    data = out.read_bytes()
    if len(data) < RAM_LEN:
        die("the client image is shorter than its region; is OUTPUT_FORMAT FULL(ram)?")
    prg.write_bytes(data[:RAM_LEN].rstrip(b"\0"))
    high.write_bytes(data[RAM_LEN:].rstrip(b"\0") or b"\0")
    return out


def build_client():
    clang = mos_clang(); lib = ensure_libc(); image, tramp = ensure_meganet()
    crypto_bin, chain_bin = build_bank()
    BIN.mkdir(exist_ok=True)
    gen = BUILD / "gen"
    srcs = sorted(str(p) for p in SRC.glob("*.c"))
    srcs += sorted(str(p) for p in (SRC / "platform").glob("*.c"))
    srcs += sorted(str(p) for p in (SRC / "m65").glob("*.c") if p.name not in HIGH_OBJS)
    srcs += [str(TLS / "tls.c")]                     # the engine; the client's half of the chain check is in HIGH, the rest is the bank's
    hi_objs = []
    for name in HIGH_OBJS:
        src = (TLS if name in ("der.c", "policy.c") else SRC / "m65") / name
        obj = gen / (src.stem + ".o")
        run([clang] + cflags() + ["-fno-lto", "-c", str(src), "-o", str(obj)])
        hi_objs.append(str(obj))
    prg = BIN / "irc.prg"; high = BIN / "high"
    print("client:")
    # the HIGH image's size is in ck_payload.h, which the client includes:
    # link once to learn it, write it, link again (the size does not move)
    link_client(clang, lib, tramp, srcs, hi_objs, prg, high)
    emit_payload(gen, BUILD / "bank" / "ck_tramp.bin", crypto_bin, chain_bin, high.stat().st_size)
    out = link_client(clang, lib, tramp, srcs, hi_objs, prg, high)
    print(f"  {prg.name}: {prg.stat().st_size} bytes; {high.name}: {high.stat().st_size} bytes (for $E000)")
    if high.stat().st_size < 1000:
        die("the HIGH image is nearly empty: irc.ld's patterns matched nothing (gemini 5.8)")
    check_rmw(out.with_suffix(".bin.elf"))
    check_headroom(BIN / "irc.map")
    c1541 = find_tool("c1541", env="C1541")
    d81 = BIN / "IRC.D81"
    shutil.copy(image, BIN / "meganet")
    shutil.copy(crypto_bin, BIN / "irccrypto")
    shutil.copy(chain_bin, BIN / "chain")
    if d81.exists():
        d81.unlink()
    run([c1541, "-format", "irc,ir", "d81", d81, "-write", prg, "irc", "-write", BIN / "meganet", "meganet",
         "-write", BIN / "irccrypto", "irccrypto", "-write", BIN / "chain", "chain", "-write", high, "high"],
        stdout=subprocess.DEVNULL)
    run([c1541, "-attach", d81, "-dir"])
    return 0


# ---- the host --------------------------------------------------------------

def host_cc():
    cc = os.environ.get("CC") or next((c for c in ("cc", "clang", "gcc") if shutil.which(c)), None)
    if not cc:
        die("no host C compiler (cc, clang or gcc); set CC")
    return cc


def host_test(name, sources, extra=()):
    out = BUILD / "host"; out.mkdir(parents=True, exist_ok=True)
    exe = out / (name + (".exe" if IS_WINDOWS else ""))
    run([host_cc()] + HOST_CFLAGS + list(extra) + ["-I", SRC] + [str(s) for s in sources] + ["-o", exe])
    return subprocess.run([str(exe)]).returncode


def crypto_sources_host():
    """Every crypto source but chacha20_small.c, which defines the same
    symbols as chacha20.c and belongs to the bank (gemini 5.4). The whole
    set goes in because crypto_equal and crypto_yield live among them:
    hand-picking a minimal list only moves the link error around."""
    return [p for p in crypto_sources() if p.name != "chacha20_small.c"]


def build_test():
    """The host suites, all of which must pass before anything reaches
    the machine. x509.c calls p256.c and rsa.c, so those link with the
    chain even though the chain itself is RSA throughout."""
    print("the IRC line protocol:")
    rc = host_test("test_irc", [ROOT / "tests" / "test_irc.c", SRC / "irc.c"])
    print("the machine's clock:")
    rc |= host_test("test_clock", [ROOT / "tests" / "test_clock.c", SRC / "clock.c"])
    print("P-256 key agreement:")
    rc |= host_test("test_ecdh", [ROOT / "tests" / "test_ecdh.c"] + crypto_sources_host(), ["-I", CRYPTO])
    print("SHA-384 and P-384, over the chain Libera served:")
    rc |= host_test("test_ec384", [ROOT / "tests" / "test_ec384.c"] + crypto_sources_host(), ["-I", CRYPTO])
    srcs = [ROOT / "tests" / "test_chain.c", TLS / "chain.c", TLS / "policy.c", TLS / "der.c", TLS / "roots.c", TLS / "x509.c"] + crypto_sources_host()
    print("certificate chain:")
    rc |= host_test("test_chain", srcs, ["-I", CRYPTO, f'-DCERT_DIR="{ROOT / "tests" / "certs"}"'])
    return rc


def tls_sources():
    return sorted(TLS.glob("*.c"))


def build_hostclient():
    """tests/tls_irc_host.c: IRC over TLS on the host, against a real
    server, the whole stack end to end before any of it goes near the
    machine. build/host/tls_irc_host HOST[:PORT] [--ip A.B.C.D]."""
    out = BUILD / "host"; out.mkdir(parents=True, exist_ok=True)
    exe = out / ("tls_irc_host" + (".exe" if IS_WINDOWS else ""))
    srcs = [ROOT / "tests" / "tls_irc_host.c", SRC / "irc.c", SRC / "clock.c"] + tls_sources() + crypto_sources_host()
    run([host_cc()] + HOST_CFLAGS + ["-I", SRC, "-I", CRYPTO] + [str(s) for s in srcs] + ["-o", exe])
    print(f"host client: {exe.relative_to(ROOT)}")
    return 0


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "client"
    if target == "client":
        return build_client()
    if target == "bank":
        build_bank(); return 0
    if target == "test":
        return build_test()
    if target == "hostclient":
        return build_hostclient()
    if target == "clean":
        shutil.rmtree(BUILD, ignore_errors=True); shutil.rmtree(BIN, ignore_errors=True); return 0
    print(__doc__); die(f"unknown target '{target}'")


if __name__ == "__main__":
    sys.exit(main())
