import subprocess as sp
import os

TARGET_DIR = "src/ebmcg"
TOOL_PATH = "tool/ebmcodegen"
if os.name == "nt":
    TOOL_PATH += ".exe"
if not os.path.exists(TOOL_PATH):
    print(f"Tool not built: {TOOL_PATH}, skipping DSL processing.")
    exit(0)
dirs = os.listdir(TARGET_DIR)
dirs = [d for d in dirs if os.path.isdir(os.path.join(TARGET_DIR, d))]
print("Processing Directories:", dirs)
for dir_name in dirs:
    dir_path = os.path.join(TARGET_DIR, dir_name)

    dsl_dir = os.path.join(dir_path, "dsl")
    output_dir = os.path.join(dir_path, "visitor", "dsl")
    if not os.path.exists(dsl_dir):
        print(f"DSL directory does not exist: {dsl_dir}")
        continue
    dsls = os.listdir(dsl_dir)
    print(f"Processing DSL files in: {dsl_dir}: {dsls}")
    dsl_updated = False
    for dsl_file in dsls:
        if not dsl_file.endswith(".dsl"):
            print(f"Skipping non-DSL file: {dsl_file}")
            continue
        template_name = dsl_file.removesuffix(".dsl")
        if not "_dsl" in template_name:
            print(f"Skipping DSL file without '_dsl' suffix: {dsl_file}")
            continue
        output_file = template_name + ".hpp"
        output_path = os.path.join(output_dir, output_file)
        dsl_path = os.path.join(dsl_dir, dsl_file)
        print(f"Processing {dsl_path}...")

        try:
            result = sp.run(
                [
                    TOOL_PATH,
                    "--mode",
                    "dsl",
                    "--dsl-file",
                    dsl_path,
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            output_content = result.stdout
        except sp.CalledProcessError as e:
            print(f"Error processing {dsl_path}: {e.stderr}")
            continue

        if os.path.exists(output_path):
            with open(output_path, "r", encoding="utf-8") as f:
                existing_content = f.read()
            if existing_content == output_content:
                print(f"No changes for {output_path}, skipping write.")
                continue

        os.makedirs(output_dir, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(output_content)
        print(f"Wrote output to {output_path}.")
        dsl_updated = True
    MAIN_FILE = os.path.join(dir_path, "main.cpp")
    if dsl_updated and os.path.exists(MAIN_FILE):
        os.utime(MAIN_FILE, None)
        print(f"Touched main.cpp to trigger rebuild: {MAIN_FILE}")
