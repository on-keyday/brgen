import os
import subprocess as sp
import json

# first, if build_config.json does not exist, copy build_config.template.json to build_config.json
if not os.path.exists("build_config.json"):
    import shutil

    shutil.copy("build_config.template.json", "build_config.json")

with open("build_config.json", "r") as f:
    BUILD_CONFIG = json.load(f)

# first, init submodules if not already done
sp.check_call(["git", "submodule", "update", "--init", "--recursive"])

# next build dependencies if not already built
FUTILS_DIR = BUILD_CONFIG.get("FUTILS_DIR", "./brgen/utils/")
BUILD_TYPE = BUILD_CONFIG.get("DEFAULT_BUILD_TYPE", "Debug")
if not os.path.exists(FUTILS_DIR) and BUILD_CONFIG.get("AUTO_SETUP_FUTILS", False):
    print("FUTILS_DIR does not exist, cloning...")
    sp.check_call(
        [
            "git",
            "clone",
            "--depth",
            "1",
            "https://github.com/on-keyday/utils",
            FUTILS_DIR,
        ]
    )
    os.chdir(FUTILS_DIR)
    print("Building utils...")
    if os.name == "nt":
        sp.check_call(
            ["build.bat", "shared", BUILD_TYPE, "futils"], cwd=FUTILS_DIR, shell=True
        )
    else:
        sp.check_call(["./build.sh", "shared", BUILD_TYPE, "futils"], cwd=FUTILS_DIR)

BRGEN_DIR = BUILD_CONFIG.get("BRGEN_DIR", "./brgen/")
if BRGEN_DIR == "./brgen/" and BUILD_CONFIG.get(
    "AUTO_SETUP_BRGEN", False
):  # this is submodule path, so already cloned
    print("Using brgen submodule.")
    os.chdir(BRGEN_DIR)
    # run build.bat or build.sh
    print("Building brgen...")
    if os.name == "nt":
        sp.check_call(["build.bat", "native", BUILD_TYPE], cwd=BRGEN_DIR, shell=True)
    else:
        sp.check_call(["./build.sh", "native", BUILD_TYPE], cwd=BRGEN_DIR)

os.environ["FUTILS_DIR"] = os.path.abspath(FUTILS_DIR)
print("FUTILS_DIR set to:", os.environ["FUTILS_DIR"])
EMSDK_DIR = BUILD_CONFIG.get("EMSDK_DIR", "./emsdk/")

if not os.path.exists(EMSDK_DIR) and BUILD_CONFIG.get("AUTO_SETUP_EMSDK", False):
    print("EMSDK_DIR does not exist, cloning...")
    sp.check_call(
        [
            "git",
            "clone",
            "--depth",
            "1",
            "https://github.com/emscripten-core/emsdk",
            EMSDK_DIR,
        ]
    )

# then build
print("Building project...")
sp.check_call(["python", "script/build.py"])
