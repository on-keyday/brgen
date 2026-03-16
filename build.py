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
    """Source the emsdk environment and merge it into the current process environment."""
    if os.name == "posix":
        emsdk_env = os.path.join(EMSDK_DIR, "emsdk_env.sh")
    elif os.name == "nt":
        emsdk_env = os.path.join(EMSDK_DIR, "emsdk_env.ps1")
    else:
        print("Unsupported OS for emsdk sourcing")
        sys.exit(1)

    emsdk_env = os.path.abspath(emsdk_env)
    copy_env = os.environ.copy()
    copy_env["EMSDK_QUIET"] = "1"

    if os.name == "posix":
        ENV = subprocess.check_output(
            ["bash", "-c", f". '{emsdk_env}' > /dev/null ; env"],
            env=copy_env,
            stderr=sys.stderr,
        )
    elif os.name == "nt":
        ENV = subprocess.check_output(
            [
                "powershell",
                "$PSDefaultParameterValues['*:Encoding'] = 'utf8'",
                ";",
                "$global:OutputEncoding = [System.Text.Encoding]::UTF8",
                ";",
                "[console]::OutputEncoding = [System.Text.Encoding]::UTF8",
                ";",
                ".",
                emsdk_env,
                "|",
                "Out-Null",
                ";",
                "Get-ChildItem Env:",
                "|",
                "ForEach-Object",
                "{",
                "$_.Name + '=' + $_.Value}",
            ],
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
    BUILD_DIR = f"./built/web/{BUILD_TYPE}"
    source_emsdk()
    os.environ["GOOS"] = "js"
    os.environ["GOARCH"] = "wasm"
    try:
        subprocess.run(
            [
                "emcmake", "cmake",
                "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={BUILD_TYPE}",
                f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}/web/dev/src",
                "-S", ".",
                "-B", BUILD_DIR,
            ],
            shell=(os.name == "nt"),
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
        if os.name == "posix":
            subprocess.run(
                ["python", "script/dirty_patch.py"],
                check=True,
                stdout=sys.stdout,
                stderr=sys.stderr,
            )
        subprocess.run(
            ["python", "script/copy_bmgen_stub.py"],
            check=True,
            stdout=sys.stdout,
            stderr=sys.stderr,
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
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["npm", "run", "build"],
        check=True,
        cwd=os.path.join("web", "dev"),
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
    set_wasmexec()
    build_wasm()
    build_npm()
    generate_ast_libs()
    install_lsp()
else:
    print(f"Invalid build mode: {BUILD_MODE}")
    sys.exit(1)

del os.environ["FUTILS_DIR"]
del os.environ["BUILD_MODE"]
del os.environ["S2J_LIB"]
del os.environ["BRGEN_RUST_ENABLED"]
