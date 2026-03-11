import os
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

INSTALL_PREFIX = os.getenv(
    "INSTALL_PREFIX",
    build_config.get("INSTALL_PREFIX", os.path.abspath(".")),
)
FUTILS_DIR = os.getenv(
    "FUTILS_DIR",
    build_config.get("FUTILS_DIR", "./brgen/utils/"),
)
BRGEN_DIR = os.getenv("BRGEN_DIR", build_config.get("BRGEN_DIR", "./brgen/"))
EMSDK_DIR = os.getenv("EMSDK_DIR", build_config.get("EMSDK_DIR", "./emsdk/"))
BUILD_OLD_BMGEN = os.getenv(
    "BUILD_OLD_BMGEN", build_config.get("BUILD_OLD_BMGEN", False)
)
if isinstance(BUILD_OLD_BMGEN, str):
    BUILD_OLD_BMGEN = BUILD_OLD_BMGEN.lower() in ("1", "true", "yes")

print("BUILD_TYPE:", BUILD_TYPE)
print("BUILD_MODE:", BUILD_MODE)
print("INSTALL_PREFIX:", INSTALL_PREFIX)
print("FUTILS_DIR:", FUTILS_DIR)
print("BRGEN_DIR:", BRGEN_DIR)
print("EMSDK_DIR:", EMSDK_DIR)

shell = os.getenv("SHELL")
if os.name == "posix":
    EMSDK_DIR = os.path.join(EMSDK_DIR, "emsdk_env.sh")
elif os.name == "nt":
    EMSDK_DIR = os.path.join(EMSDK_DIR, "emsdk_env.ps1")
else:
    EMSDK_DIR = os.path.join(EMSDK_DIR, "emsdk_env.sh")

EMSDK_DIR = os.path.abspath(EMSDK_DIR)

DLL_EXT = ".dll" if os.name == "nt" else ".so"

os.environ["FUTILS_DIR"] = FUTILS_DIR
os.environ["BUILD_MODE"] = BUILD_MODE
if os.path.exists(os.path.join(BRGEN_DIR, "tool", "libs2j" + DLL_EXT)):
    os.environ["BRGEN_DIR"] = BRGEN_DIR
else:
    print(f"BRGEN_DIR does not contain tool/libs2j{DLL_EXT}, unset BRGEN_DIR")
    if "BRGEN_DIR" in os.environ:
        del os.environ["BRGEN_DIR"]
os.environ["BUILD_OLD_BMGEN"] = "1" if BUILD_OLD_BMGEN else "0"


def source_emsdk():
    copyEnv = os.environ.copy()
    copyEnv["EMSDK_QUIET"] = "1"
    if os.name == "posix":
        ENV = subprocess.check_output(
            [".", EMSDK_DIR, ">", "/dev/null", ";", "env"],
            shell=True,
            env=copyEnv,
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
                EMSDK_DIR,
                "|",
                "Out-Null",
                ";",
                "Get-ChildItem Env:",
                "|",
                "ForEach-Object",
                "{",
                "$_.Name + '=' + $_.Value}",
            ],
            env=copyEnv,
            stderr=sys.stderr,
        )
    else:
        print("Unsupported environment")
        sys.exit(1)
    for line in ENV.splitlines():
        try:
            line = line.decode()
        except UnicodeDecodeError as e:
            print(e.start, e.end, line[e.start - 10 : e.end + 10])
            raise e
        if "=" in line:
            key, value = line.split("=", 1)
            os.environ[key] = value


if BUILD_MODE == "native":
    subprocess.run(
        [
            "cmake",
            "-D",
            "CMAKE_CXX_COMPILER=clang++",
            "-D",
            "CMAKE_C_COMPILER=clang",
            "-G",
            "Ninja",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}",
            f"-D",
            f"CMAKE_BUILD_TYPE={BUILD_TYPE}",
            "-S",
            ".",
            f"-B",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["cmake", "--build", f"./built/{BUILD_MODE}/{BUILD_TYPE}"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "Unspecified",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "gen_template",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
elif BUILD_MODE == "web":
    source_emsdk()
    subprocess.run(
        [
            "emcmake",
            "cmake",
            "-G",
            "Ninja",
            f"-D",
            f"CMAKE_BUILD_TYPE={BUILD_TYPE}",
            f"-DCMAKE_INSTALL_PREFIX={INSTALL_PREFIX}/web",
            "-S",
            ".",
            f"-B",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
        ],
        shell=os.name == "nt",
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        ["cmake", "--build", f"./built/{BUILD_MODE}/{BUILD_TYPE}"],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
    subprocess.run(
        [
            "cmake",
            "--install",
            f"./built/{BUILD_MODE}/{BUILD_TYPE}",
            "--component",
            "Unspecified",
        ],
        check=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
    )
else:
    print("Invalid build mode")
