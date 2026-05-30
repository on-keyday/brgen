#!/usr/bin/env python3
# Test logic for ebm2wuffs.
#
# Scope: drive the generated .wuffs through `wuffs gen` (parse + type check +
# bounds/overflow prover + C codegen), then run a decode round-trip on the
# emitted C. The streaming decode writes a token_writer; the harness rebuilds
# the source bytes from the emitted tokens' lengths and writes them to
# OUTPUT_FILE for the framework to compare against INPUT_FILE.
#
# For a byte-vector-only format (e.g. until_eof) this reproduces INPUT exactly.
# Formats with scalar fields do not yet match: scalar values are stored in the
# struct, not carried in tokens, so they are absent from the rebuilt bytes. That
# is a known gap, tracked separately. Encode is stubbed in the streaming model,
# so there is no encode round-trip.
import os
import pathlib
import shutil
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).parent

# Stale-file cleanup threshold (seconds). At startup we sweep gen/c/ of any
# wuffs-std-ebm2wuffsprobe*.c files older than this -- they are leftovers
# from prior unictest runs (we deliberately do not unlink our own at the
# end; see the comment in main()).
_STALE_PROBE_SECONDS = 3600


def cleanup_stale_probes(wuffs_root):
    """Remove wuffs-std-ebm2wuffsprobe*.c files older than _STALE_PROBE_SECONDS.

    The per-test cleanup that unlinks the current .c was the source of a
    flaky-test race: another parallel test's `wuffs gen` enumerates gen/c/
    via genreleaseLang() (wuffs/cmd/wuffs/release.go:41) and then opens each
    listed file via the wuffs-c genrelease subprocess. If our unlink runs
    between the two steps, the subprocess fails with "system cannot find
    the file specified" -- causing a green wuffs gen to be reported as a
    failure. We therefore keep our .c around for the duration of the run and
    only collect stale files at startup, scoped to our own probe prefix so
    we never touch unrelated wuffs std libraries.
    """
    gen_c = wuffs_root / "gen" / "c"
    if not gen_c.exists():
        return
    now = time.time()
    for f in gen_c.glob("wuffs-std-ebm2wuffsprobe*.c"):
        try:
            if now - f.stat().st_mtime > _STALE_PROBE_SECONDS:
                f.unlink()
        except OSError:
            pass

# Decode round-trip harness, parameterized by the throwaway package name and the
# format (struct) name. Placeholders are substituted (not %-formatted) so the C
# printf "%s" specifiers are left intact. The package C #includes ./wuffs-base.c
# itself; in NONMONOLITHIC mode each module's implementation must be enabled
# explicitly, hence the BASE + package MODULE defines.
MAIN_C_TEMPLATE = r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__%PKG_UPPER%
#include "%PKG_C_PATH%"

typedef wuffs_%PKG%__%FORMAT% PkgStruct;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <input> <output>\n", argv[0]);
        return 1;
    }
    FILE* fin = fopen(argv[1], "rb");
    if (!fin) { fprintf(stderr, "open input failed\n"); return 1; }
    fseek(fin, 0, SEEK_END);
    long in_len = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    uint8_t* in_buf = (uint8_t*)malloc(in_len ? (size_t)in_len : 1);
    if (fread(in_buf, 1, (size_t)in_len, fin) != (size_t)in_len) {
        fprintf(stderr, "read input failed\n"); fclose(fin); return 1;
    }
    fclose(fin);

    // src: the whole input, closed (no more bytes will arrive).
    wuffs_base__io_buffer src = wuffs_base__make_io_buffer(
        wuffs_base__make_slice_u8(in_buf, (size_t)in_len),
        wuffs_base__make_io_buffer_meta((size_t)in_len, 0, 0, true));

    wuffs_base__token tok_array[4096];
    wuffs_base__token_buffer dst = wuffs_base__make_token_buffer(
        wuffs_base__make_slice_token(tok_array, 4096),
        wuffs_base__make_token_buffer_meta(0, 0, 0, false));

    PkgStruct* self = (PkgStruct*)malloc(sizeof__wuffs_%PKG%__%FORMAT%());
    wuffs_base__status ist = wuffs_%PKG%__%FORMAT%__initialize(
        self, sizeof__wuffs_%PKG%__%FORMAT%(), WUFFS_VERSION, 0);
    if (!wuffs_base__status__is_ok(&ist)) {
        fprintf(stderr, "initialize failed: %s\n", wuffs_base__status__message(&ist));
        return 1;
    }

    // Rebuild the source bytes from the emitted tokens: each token's length is
    // the number of src bytes it represents, in order.
    uint8_t* out_buf = (uint8_t*)malloc(in_len ? (size_t)in_len : 1);
    size_t out_len = 0;
    size_t src_token_pos = 0;

    while (1) {
        wuffs_base__status st = wuffs_%PKG%__%FORMAT%__decode(self, &dst, &src);
        while (dst.meta.ri < dst.meta.wi) {
            wuffs_base__token* t = &dst.data.ptr[dst.meta.ri++];
            uint64_t len = wuffs_base__token__length(t);
            if (src_token_pos + (size_t)len > (size_t)in_len) {
                fprintf(stderr, "token length overruns input\n");
                return 30;
            }
            memcpy(out_buf + out_len, in_buf + src_token_pos, (size_t)len);
            src_token_pos += (size_t)len;
            out_len += (size_t)len;
        }
        wuffs_base__token_buffer__compact(&dst);
        if (wuffs_base__status__is_ok(&st)) { break; }
        if (st.repr == wuffs_base__suspension__short_write) { continue; }
        if (st.repr == wuffs_base__suspension__short_read) {
            fprintf(stderr, "unexpected short read (src closed)\n");
            return 10;
        }
        fprintf(stderr, "decode error: %s\n", wuffs_base__status__message(&st));
        return 10;
    }

    FILE* fout = fopen(argv[2], "wb");
    if (!fout) { fprintf(stderr, "open output failed\n"); return 1; }
    fwrite(out_buf, 1, out_len, fout);
    fclose(fout);
    return 0;
}
'''


def resolve_wuffs_root():
    candidates = []
    env_root = os.environ.get("WUFFS_ROOT")
    if env_root:
        candidates.append(pathlib.Path(env_root))
    candidates.append(HERE / ".wuffs")  # created by dependency_setup.py
    for c in candidates:
        if (c / "wuffs-root-directory.txt").exists():
            return c.resolve()
    return None


def main():
    TEST_TARGET_FILE = sys.argv[1]  # the generated .wuffs file
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]  # struct name (PascalCase)

    print(f"Testing {TEST_TARGET_FILE} (format={TEST_TARGET_FORMAT})")

    wuffs_root = resolve_wuffs_root()
    if wuffs_root is None:
        print("Wuffs toolchain not found. Run this runner's dependency_setup.py "
              "or set WUFFS_ROOT to a wuffs checkout.")
        sys.exit(1)

    # Sweep stale probe .c files from prior runs before doing anything else.
    # See cleanup_stale_probes() docstring for the race this works around.
    cleanup_stale_probes(wuffs_root)

    env = os.environ.copy()
    gopath = subprocess.check_output(["go", "env", "GOPATH"], text=True).strip()
    env["PATH"] = env.get("PATH", "") + os.pathsep + str(pathlib.Path(gopath) / "bin")
    wuffs_exe = "wuffs.exe" if os.name == "nt" else "wuffs"

    # `wuffs gen` operates on a package directory under <root>/std. Stage the
    # generated file as a throwaway package (unique per process to avoid clashes).
    # Wuffs package names must match [a-z0-9]+ (no underscores).
    pkg = f"ebm2wuffsprobe{os.getpid()}"
    pkgdir = wuffs_root / "std" / pkg
    try:
        if pkgdir.exists():
            shutil.rmtree(pkgdir)
        pkgdir.mkdir(parents=True)
        shutil.copy2(TEST_TARGET_FILE, pkgdir / f"{pkg}.wuffs")

        print(f"\nRunning `wuffs gen` on std/{pkg} ...")
        proc = subprocess.run(
            [wuffs_exe, "gen", "-langs", "c", f"std/{pkg}"],
            cwd=str(wuffs_root),
            env=env,
            capture_output=True,
            text=True,
        )
        if proc.stdout:
            print("--- wuffs gen stdout ---")
            print(proc.stdout)
        if proc.stderr:
            print("--- wuffs gen stderr ---")
            print(proc.stderr)
        if proc.returncode != 0:
            print("wuffs gen failed: generated Wuffs did not pass the type "
                  "checker / bounds prover.")
            # NOT exit 10: gen failure is a codegen-pipeline internal error,
            # not the decoder rejecting bad input. Exit 10 (decoder failure)
            # would let failure-case inputs PASS spuriously on the same broken
            # .wuffs that makes valid inputs FAIL.
            sys.exit(1)
        print("wuffs gen succeeded: generated Wuffs compiles to C.")
    finally:
        if pkgdir.exists():
            shutil.rmtree(pkgdir, ignore_errors=True)

    # --- decode round-trip on the emitted C ---
    # `wuffs gen` wrote the package implementation to gen/c/wuffs-std-<pkg>.c
    # (the std/<pkg> source dir was removed above, but gen/c survives). Compile a
    # small main() against it and rebuild the source bytes from the tokens.
    pkg_c = wuffs_root / "gen" / "c" / f"wuffs-std-{pkg}.c"
    if not pkg_c.exists():
        print(f"expected generated package C not found: {pkg_c}")
        sys.exit(1)
    try:
        build_dir = pathlib.Path(OUTPUT_FILE).resolve().parent
        main_c = build_dir / "wuffs_rt_main.c"
        exe = build_dir / ("wuffs_rt.exe" if os.name == "nt" else "wuffs_rt")
        src = (MAIN_C_TEMPLATE
               .replace("%PKG_C_PATH%", pkg_c.as_posix())
               .replace("%PKG_UPPER%", pkg.upper())
               .replace("%PKG%", pkg)
               .replace("%FORMAT%", TEST_TARGET_FORMAT))
        main_c.write_text(src)

        print("\nCompiling decode round-trip harness ...")
        cproc = subprocess.run(
            ["clang", "-O0", "-D_CRT_SECURE_NO_WARNINGS",
             str(main_c), "-o", str(exe)],
            capture_output=True, text=True,
        )
        if cproc.stdout:
            print(cproc.stdout)
        if cproc.stderr:
            print(cproc.stderr)
        if cproc.returncode != 0:
            print("decode harness compile failed.")
            sys.exit(1)

        print(f"Running decode round-trip: {exe}")
        rproc = subprocess.run([str(exe), INPUT_FILE, OUTPUT_FILE],
                               stdout=sys.stdout, stderr=sys.stderr)
        # Exit code contract matches the other runners: 0 = decode ok (framework
        # then compares OUTPUT_FILE to INPUT_FILE), 10 = decode failed.
        sys.exit(rproc.returncode)
    finally:
        # Deliberately do NOT unlink pkg_c here. Other parallel test
        # processes may currently be running `wuffs gen`, which internally
        # enumerates gen/c/ via wuffs/cmd/wuffs/release.go:findFiles and
        # then opens each file from a wuffs-c genrelease subprocess. If our
        # unlink ran between the two steps, the subprocess would fail with
        # "system cannot find the file specified" -- spuriously turning a
        # passing wuffs gen into a unictest FAIL. Leaving the file around
        # means concurrent genreleases see a stable file list. We sweep
        # stale files at startup of the next run (cleanup_stale_probes).
        pass


if __name__ == "__main__":
    main()
