import os
import platform
import subprocess
import sys
import json

try:
    with open("build_config.json", "r") as f:
        build_config = json.load(f)
    print("Loaded build_config.json")
except FileNotFoundError:
    print("build_config.json not found, using defaults")
    build_config = {}

print("Building...", sys.argv)
BUILD_MODE = (
    sys.argv[1]
    if len(sys.argv) > 1
    else build_config.get("DEFAULT_BUILD_MODE", "native")
)
BUILD_TYPE = (
    sys.argv[2]
    if len(sys.argv) > 2
    else build_config.get(
        (
            "DEFAULT_BUILD_TYPE_WEB"
            if BUILD_MODE == "web"
            else "DEFAULT_BUILD_TYPE_NATIVE"
        ),
        "Debug",
    )
)

FUTILS_DIR = os.getenv(
    "FUTILS_DIR",
    build_config.get("FUTILS_DIR", "./utils/"),
)
EMSDK_DIR = os.getenv("EMSDK_DIR", build_config.get("EMSDK_DIR", "./emsdk/"))
INSTALL_PREFIX = os.getenv(
    "INSTALL_PREFIX",
    build_config.get("INSTALL_PREFIX", "."),
)
PARALLEL_BUILD = os.getenv("PARALLEL_BUILD", build_config.get("PARALLEL_BUILD", ""))

CXX_COMPILER = os.getenv("FUTILS_CXX_COMPILER", "clang++")
C_COMPILER = os.getenv("FUTILS_C_COMPILER", "clang")

IS_MACOS = platform.system() == "Darwin"
S2J_LIB = "0" if IS_MACOS else "1"

print("BUILD_MODE:", BUILD_MODE)
print("BUILD_TYPE:", BUILD_TYPE)
print("FUTILS_DIR:", FUTILS_DIR)
print("EMSDK_DIR:", EMSDK_DIR)
print("INSTALL_PREFIX:", INSTALL_PREFIX)
print("CXX_COMPILER:", CXX_COMPILER)
print("C_COMPILER:", C_COMPILER)
print("S2J_LIB:", S2J_LIB)

os.environ["S2J_LIB"] = S2J_LIB
os.environ["BRGEN_RUST_ENABLED"] = "1"
os.environ["FUTILS_DIR"] = os.path.abspath(FUTILS_DIR)
os.environ["BUILD_MODE"] = BUILD_MODE


def clone_utils():
    """Clone and build futils if utils/ directory does not exist."""
    print("Cloning utils...")
    subprocess.run(
        ["git", "clone", "https://github.com/on-keyday/utils", "--depth", "1"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    orig_dir = os.getcwd()
    os.chdir("utils")
    try:
        build_target = "wasm-em" if BUILD_MODE == "web" else "shared"
        subprocess.run(
            ["bash", "build", build_target, BUILD_TYPE, "futils"],
            check=True,
            stdout=sys.stdout,
            stderr=sys.stderr,
        )
        if os.getenv("S2J_USE_NETWORK") == "1" and BUILD_MODE != "web":
            subprocess.run(
                ["bash", "build", "shared", BUILD_TYPE, "fnet"],
                check=True,
                stdout=sys.stdout,
                stderr=sys.stderr,
            )
            subprocess.run(
                ["bash", "build", "shared", BUILD_TYPE, "fnetserv"],
                check=True,
                stdout=sys.stdout,
                stderr=sys.stderr,
            )
    finally:
        os.chdir(orig_dir)


def source_emsdk():
    """Source the emsdk environment and merge it into the current process environment.
    Used on POSIX only. On Windows, emsdk is sourced inline per-command via cmd_run()."""
    emsdk_env = os.path.abspath(os.path.join(EMSDK_DIR, "emsdk_env.sh"))
    copy_env = os.environ.copy()
    copy_env["EMSDK_QUIET"] = "1"
    ENV = subprocess.check_output(
        ["bash", "-c", f". '{emsdk_env}' > /dev/null ; env"],
        env=copy_env,
        stderr=sys.stderr,
    )
    for line in ENV.splitlines():
        try:
            line = line.decode()
        except UnicodeDecodeError as e:
            print(e.start, e.end, line[e.start - 10 : e.end + 10])
            raise e
        if "=" in line:
            key, value = line.split("=", 1)
            os.environ[key] = value


def _msys_to_win(p):
    """Convert /c/foo/bar -> C:\\foo\\bar (MSYS path to Windows path)."""
    import re
    m = re.match(r"^/([a-zA-Z])(/.*)$", p)
    if m:
        return f"{m.group(1).upper()}:{m.group(2).replace('/', chr(92))}"
    return p


def _win_path_env():
    """Return PATH with all MSYS-style entries converted to Windows style.
    Needed so emsdk_env.bat detects a Windows environment, not MSYS.
    Handles both colon-separated MSYS PATH and Windows drive letter colons."""
    raw = os.environ.get("PATH", "")
    # Split on ":" but preserve drive letters (single letter before ":")
    segments = raw.split(":")
    parts = []
    i = 0
    while i < len(segments):
        s = segments[i]
        if len(s) == 1 and s.isalpha() and i + 1 < len(segments):
            # Windows drive letter split across ":", rejoin as "C:/..."
            parts.append(s + ":" + segments[i + 1])
            i += 2
        else:
            if s:
                parts.append(s)
            i += 1
    seen = []
    for p in parts:
        w = _msys_to_win(p.strip())
        if w not in seen:
            seen.append(w)
    return ";".join(seen)


def cmd_run(cmd_str, **kwargs):
    """Run a cmd.exe command chain on Windows with emsdk pre-sourced via call.
    Strips MSYS markers so emsdk_env.bat detects a Windows environment."""
    emsdk_bat = os.path.abspath(os.path.join(EMSDK_DIR, "emsdk_env.bat"))
    env = os.environ.copy()
    env["PATH"] = _win_path_env()
    # Remove MSYS/Cygwin markers that would make emsdk_env.bat emit bash-style output
    for key in ("MSYSTEM", "MSYS", "MSYS2_PATH_TYPE", "CYGWIN", "SHELL"):
        env.pop(key, None)
    return subprocess.run(
        f'call "{emsdk_bat}" && {cmd_str}',
        shell=True,
        env=env,
        **kwargs,
    )


def build_native():
    BUILD_DIR = f"./built/native/{BUILD_TYPE}"
    subprocess.run(
        [
            "cmake",
            "-D", f"CMAKE_CXX_COMPILER={CXX_COMPILER}",
            "-D", f"CMAKE_C_COMPILER={C_COMPILER}",
            "-G", "Ninja",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}",
            f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}",
            "-S", ".",
            "-B", BUILD_DIR,
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    ninja_cmd = ["ninja", "-C", BUILD_DIR]
    if PARALLEL_BUILD:
        ninja_cmd += ["-j", str(PARALLEL_BUILD)]
    subprocess.run(ninja_cmd, check=True, stdout=sys.stdout, stderr=sys.stderr)
    subprocess.run(
        ["ninja", "-C", BUILD_DIR, "install"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )


def build_wasm():
    """Emscripten cmake+ninja build and copy WASM artifacts into web/dev/src."""
    set_wasmexec()
    BUILD_DIR = f"./built/web/{BUILD_TYPE}"
    os.environ["GOOS"] = "js"
    os.environ["GOARCH"] = "wasm"
    os.environ["BUILD_MODE"] = "web" # Tell CMakeLists.txt to set Wasm-specific flags and install targets
    try:
        if os.name == "nt":
            # Windows: source emsdk inline via PowerShell for each command
            parallel = f" -j {PARALLEL_BUILD}" if PARALLEL_BUILD else ""
            cmd_run(
                f'emcmake cmake -G Ninja "-DCMAKE_BUILD_TYPE={BUILD_TYPE}"'
                f' "-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}/web/dev/src" -S . -B {BUILD_DIR}',
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )
            cmd_run(
                f"ninja -C {BUILD_DIR}{parallel}",
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )
            cmd_run(
                f"ninja -C {BUILD_DIR} install",
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )
        else:
            # POSIX (Linux/macOS): source emsdk into current env, then run normally
            source_emsdk()
            subprocess.run(
                [
                    "emcmake", "cmake",
                    "-G", "Ninja",
                    f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}",
                    f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}/web/dev/src",
                    "-S", ".",
                    "-B", BUILD_DIR,
                ],
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )
            ninja_cmd = ["ninja", "-C", BUILD_DIR]
            if PARALLEL_BUILD:
                ninja_cmd += ["-j", str(PARALLEL_BUILD)]
            subprocess.run(ninja_cmd, check=True, stdout=sys.stdout, stderr=sys.stderr)
            subprocess.run(
                ["ninja", "-C", BUILD_DIR, "install"],
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )

        if os.name == "posix":
            subprocess.run(
                ["python", "script/dirty_patch.py"],
                check=True, stdout=sys.stdout, stderr=sys.stderr,
            )
        subprocess.run(
            ["python", "script/copy_bmgen_stub.py"],
            check=True, stdout=sys.stdout, stderr=sys.stderr,
        )
    finally:
        del os.environ["GOOS"]
        del os.environ["GOARCH"]


def build_npm():
    """npm install + npm run build + copy examples."""
    subprocess.run(
        ["npm", "install"],
        check=True,
        cwd=os.path.join("web", "dev"),
        shell=(os.name == "nt"),
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["npm", "run", "build"],
        check=True,
        cwd=os.path.join("web", "dev"),
        shell=(os.name == "nt"),
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["python", "script/copy_example.py"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )


def set_wasmexec():
    """Resolve GOROOT via `go env` and set WASMEXEC_FILE environment variable."""
    result = subprocess.run(
        ["go", "env", "GOROOT"],
        check=True,
        capture_output=True,
        text=True,
    )
    goroot = result.stdout.strip()
    wasmexec = os.path.join(goroot, "lib", "wasm", "wasm_exec.js")
    os.environ["WASMEXEC_FILE"] = wasmexec
    print("WASMEXEC_FILE:", wasmexec)


def generate_ast_libs():
    """Regenerate all AST libraries (Go, TypeScript, Rust, Python, C, C#, Dart)."""
    subprocess.run(
        ["python", "script/generate.py"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )


def install_lsp():
    """Package and install the brgen VSCode LSP extension."""
    subprocess.run(
        ["vsce", "package"],
        check=True,
        cwd="lsp",
        shell=(os.name == "nt"),
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["code", "--install-extension", "./brgen-lsp-0.0.1.vsix"],
        check=True,
        cwd="lsp",
        shell=(os.name == "nt"),
        stdout=sys.stdout,
        stderr=sys.stderr,
    )


# Ensure futils is available
if BUILD_MODE not in ("npm", "generate", "lsp"):
    if not os.path.isdir(FUTILS_DIR):
        clone_utils()
        FUTILS_DIR = "./utils"
        os.environ["FUTILS_DIR"] = os.path.abspath(FUTILS_DIR)

# Download Go module dependencies
if BUILD_MODE in ("native", "wasm", "web", "all"):
    subprocess.run(
        ["go", "mod", "download"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )

if BUILD_MODE == "native":
    build_native()
elif BUILD_MODE == "wasm":
    build_wasm()
elif BUILD_MODE == "npm":
    build_npm()
elif BUILD_MODE == "web":
    build_wasm()
    build_npm()
elif BUILD_MODE == "generate":
    generate_ast_libs()
elif BUILD_MODE == "lsp":
    install_lsp()
elif BUILD_MODE == "all":
    build_native()
    build_wasm()
    build_npm()
    generate_ast_libs()
    install_lsp()
else:
    print(f"Invalid build mode: {BUILD_MODE}")
    sys.exit(1)


