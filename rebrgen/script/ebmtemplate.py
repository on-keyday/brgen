import json
import subprocess
import sys
import os
import re

# This script simplifies calling the ebmcodegen tool to generate templates.
# It can generate a single template, test generation for all available templates,
# or update existing hook files with the latest templates.

START_MARKER = "/*DO NOT EDIT BELOW SECTION MANUALLY*/\n"
END_MARKER = "/*DO NOT EDIT ABOVE SECTION MANUALLY*/\n"


def get_tool_path():
    """Determine the ebmcodegen tool path based on the OS."""
    tool_path = "tool/ebmcodegen.exe" if sys.platform == "win32" else "tool/ebmcodegen"
    if not os.path.exists(tool_path):
        print(
            f"Error: '{tool_path}' not found. Please build the project first using 'python script/build.py native Debug'.",
            file=sys.stderr,
        )
        return None
    return tool_path


def run_single_template(tool_path, template_target):
    """Generate a single template and print it to stdout."""
    command = [tool_path, "--mode", "template", "--template-target", template_target]
    print(f"Running command: {' '.join(command)}", file=sys.stderr)
    try:
        subprocess.run(command, check=True, text=True, encoding="utf-8")
    except subprocess.CalledProcessError as e:
        print(
            f"Error: ebmcodegen failed for target '{template_target}' with exit code {e.returncode}",
            file=sys.stderr,
        )
        sys.exit(1)


def get_mode_dir(gmode: str) -> str:
    if gmode == "interpret":
        return "ebmip"
    else:
        return "ebmcg"


def run_save_template(tool_path, template_target, gmode, lang):
    """Generate a template and save it to the specified language's visitor directory."""
    isInterpreter = gmode == "interpret"
    if lang == "default":
        lang_dir = None
        visitor_dir = f"src/ebmcodegen/default_{gmode}_visitor/visitor"
    else:
        lang_dir = f"src/{get_mode_dir(gmode)}/ebm2{lang}"
        if not os.path.isdir(lang_dir):
            print(f"Error: Directory '{lang_dir}' does not exist.", file=sys.stderr)
            sys.exit(1)
        visitor_dir = os.path.join(lang_dir, "visitor")
    isDSL = "_dsl" in template_target
    isClass = "_class" in template_target
    suffix = ".hpp"
    DSL_CODE_PREFIX = ""
    DSL_CODE_SUFFIX = ""
    if isDSL:  # if DSL template, save to dsl subdirectory
        visitor_dir = os.path.join(lang_dir, "dsl")
        suffix = ".dsl"
        DSL_CODE_PREFIX = "{%\n"
        DSL_CODE_SUFFIX = "%}\n"

    os.makedirs(visitor_dir, exist_ok=True)  # Ensure visitor directory exists

    output_path = os.path.join(visitor_dir, f"{template_target}{suffix}")
    if os.path.exists(output_path):
        print(f"Error: File '{output_path}' already exists.", file=sys.stderr)
        sys.exit(1)

    command = [tool_path, "--mode", "template", "--template-target", template_target]
    print(f"Running command: {' '.join(command)}", file=sys.stderr)
    try:
        result = subprocess.run(
            command, check=True, capture_output=True, text=True, encoding="utf-8"
        )
        DEFAULT_IMPL = "/*here to write the hook*/"
        if isClass:
            class_name = template_target.removesuffix("_class").removesuffix("_dsl")
            is_observer = any(
                x in class_name for x in ("_after", "_before", "pre_", "post_")
            )
            DEFAULT_IMPL = f"""
#include {"\"../codegen.hpp\"" if not isDSL else "\"../../codegen.hpp\""}
DEFINE_VISITOR({class_name}) {{
    using namespace CODEGEN_NAMESPACE;
    /*here to write the hook*/
    return {"pass" if is_observer else "{}" if isInterpreter else "\"\""};
}}
"""
        # Add markers to the template content
        template_content = f"{DSL_CODE_PREFIX}{START_MARKER}{result.stdout}{END_MARKER}\n{DEFAULT_IMPL}\n{DSL_CODE_SUFFIX}"
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(template_content)
        print(f"Success! Template '{template_target}' saved to '{output_path}'")
        if lang_dir:
            # touch main file
            main_file = os.path.join(lang_dir, "main.cpp")
            if os.path.exists(main_file):
                os.utime(main_file, None)
                print(f"Touched '{main_file}' to update its timestamp.")
        else:
            # list ebmcg or ebmip directory and touch all main.cpp files
            parent_dir = f"src/{get_mode_dir(gmode)}"
            for entry in os.listdir(parent_dir):
                entry_path = os.path.join(parent_dir, entry)
                if os.path.isdir(entry_path) and entry.startswith("ebm2"):
                    main_file = os.path.join(entry_path, "main.cpp")
                    if os.path.exists(main_file):
                        os.utime(main_file, None)
                        print(f"Touched '{main_file}' to update its timestamp.")
    except subprocess.CalledProcessError as e:
        print(
            f"Error: ebmcodegen failed for target '{template_target}' with exit code {e.returncode}",
            file=sys.stderr,
        )
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        sys.exit(1)


def isDSLSuitableDir(dir: str) -> bool:
    """Check if the given directory is suitable for DSL templates."""
    return dir.endswith("dsl") or dir.endswith("dsl/") or dir.endswith("dsl\\")


def isSuitableDir(name: str, dir: str) -> bool:
    """Check if the given file is suitable for DSL templates."""
    isDSLTemplate = name.endswith("_dsl")
    isDSLDir = isDSLSuitableDir(dir)
    return isDSLTemplate == isDSLDir


def list_templates(
    dir: str, strict_set: set[str], suffix: str = ".hpp"
) -> tuple[list[dict[str, str]], list[str]]:
    """List all .hpp files in the given directory and its subdirectories."""
    hpp_files = []
    non_template_files = []
    for root, _, files in os.walk(dir):
        for file in files:
            if file.endswith(suffix):
                name = os.path.splitext(file)[0]
                if name in strict_set and isSuitableDir(name, root):
                    hpp_files.append(
                        {
                            "name": name,
                            "path": os.path.join(root, file),
                        }
                    )
                else:
                    non_template_files.append(os.path.join(root, file))
    return hpp_files, non_template_files


def run_update_hooks(tool_path, lang, gmode):
    """Update all existing hook files in a language directory."""
    if lang == "all":
        try:
            with open("build_config.json") as f:
                build_config = json.load(f)
            target_languages = build_config.get(
                str(gmode).upper() + "_TARGET_LANGUAGE", []
            ) + [f"default"]
            if not target_languages:
                print(
                    "Error: No target languages defined in build_config.json.",
                    file=sys.stderr,
                )
                sys.exit(1)
            for lang in target_languages:
                if lang == "all":
                    continue  # avoid infinity recursion
                run_update_hooks(tool_path, lang, gmode)
            return
        except Exception as e:
            print(f"Error: Failed to read build_config.json: {e}", file=sys.stderr)
            sys.exit(1)

    if lang == "default":
        visitor_dir = f"src/ebmcodegen/default_{gmode}_visitor/visitor"
        dsl_dir = None
    else:
        visitor_dir = os.path.join("src", get_mode_dir(gmode), f"ebm2{lang}", "visitor")
        dsl_dir = os.path.join("src", get_mode_dir(gmode), f"ebm2{lang}", "dsl")
    if not os.path.isdir(visitor_dir):
        print(f"Error: Visitor directory '{visitor_dir}' not found.", file=sys.stderr)
        sys.exit(1)

    print(f"Checking for updates in '{visitor_dir}'...")
    hpp_files, _ = list_templates(visitor_dir, set(get_available_templates()))
    dsl_files, _ = (
        list_templates(dsl_dir, set(get_available_templates()), suffix=".dsl")
        if dsl_dir
        else ([], [])
    )

    if not hpp_files and not dsl_files:
        print("No .hpp or .dsl files found to update.", file=sys.stderr)
        return
    targets = hpp_files + dsl_files
    for file in targets:
        template_target = file["name"]
        file_path = file["path"]
        print(f"-- Processing '{file_path}'...", file=sys.stderr)

        try:
            # Get the canonical template content
            command = [
                tool_path,
                "--mode",
                "template",
                "--template-target",
                template_target,
            ]
            result = subprocess.run(
                command, check=True, capture_output=True, text=True, encoding="utf-8"
            )
            new_body = result.stdout
            new_block = f"{START_MARKER}{new_body}{END_MARKER}"

            with open(file_path, "r+", encoding="utf-8") as f:
                existing_content = f.read()
                # Use regex to find the existing block
                pattern = re.compile(
                    f"^{re.escape(START_MARKER)}(.*?)^{re.escape(END_MARKER)}",
                    re.DOTALL | re.MULTILINE,
                )
                match = pattern.search(existing_content)

                if not match:
                    # Case 1: Marker not found, add to the top
                    print(
                        "   -> No template block found. Prepending...", file=sys.stderr
                    )
                    new_content = f"{new_block}{existing_content}"
                    f.seek(0)
                    f.write(new_content)
                    f.truncate()
                else:
                    # Case 2: Marker found, check for differences
                    existing_body = match.group(1)
                    if existing_body != new_body:
                        print("   -> Template differs. Updating...", file=sys.stderr)
                        new_content = pattern.sub(new_block, existing_content)
                        f.seek(0)
                        f.write(new_content)
                        f.truncate()
                    else:
                        print("   -> Already up-to-date.", file=sys.stderr)

        except subprocess.CalledProcessError as e:
            print(
                f"   -> Error processing target '{template_target}': {e.stderr}",
                file=sys.stderr,
            )
        except Exception as e:
            print(f"   -> An unexpected error occurred: {e}", file=sys.stderr)

    print("\nUpdate check complete.")


def run_test_mode(tool_path):
    """Get all template targets and test generation for each one."""
    print(
        "Running in test mode. Fetching all available template targets...",
        file=sys.stderr,
    )

    # Get the list of hooks
    targets = get_available_templates()
    if not targets:
        print("No available templates found. Exiting test mode.", file=sys.stderr)
        sys.exit(1)

    # Test each target
    for i, target in enumerate(targets):
        print(f"[{i+1}/{len(targets)}] Testing target: {target}", file=sys.stderr)
        command = [tool_path, "--mode", "template", "--template-target", target]
        try:
            # We don't need to see the output, just check if it succeeds
            subprocess.run(
                command, check=True, capture_output=True, text=True, encoding="utf-8"
            )
        except subprocess.CalledProcessError as e:
            print(
                f"\n--- ERROR: Test failed for target: '{target}' ---", file=sys.stderr
            )
            print(f"ebmcodegen exited with code {e.returncode}", file=sys.stderr)
            print("\n--- STDOUT ---", file=sys.stderr)
            print(e.stdout, file=sys.stderr)
            print("\n--- STDERR ---", file=sys.stderr)
            print(e.stderr, file=sys.stderr)
            print("\n--- Test stopped. ---", file=sys.stderr)
            sys.exit(1)

    print("\nSuccess! All template targets generated successfully.", file=sys.stderr)


def get_available_templates() -> list[str]:
    """List all available template targets."""
    tool_path = get_tool_path()
    if not tool_path:
        return []
    try:
        hooklist_cmd = [tool_path, "--mode", "hooklist"]
        result = subprocess.run(
            hooklist_cmd, check=True, capture_output=True, text=True, encoding="utf-8"
        )
        targets = [line for line in result.stdout.splitlines() if line.strip()]
        return targets
    except subprocess.CalledProcessError as e:
        print(
            f"Error: Failed to get hooklist. ebmcodegen exited with code {e.returncode}",
            file=sys.stderr,
        )
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        return []


def list_defined_templates(lang: str, gmode: str):
    """List all defined templates for a given language."""
    if lang == "default":
        visitor_dir = f"src/ebmcodegen/default_{gmode}_visitor/visitor"
    else:
        visitor_dir = os.path.join("src", get_mode_dir(gmode), f"ebm2{lang}", "visitor")
    if not os.path.isdir(visitor_dir):
        print(f"Error: Visitor directory '{visitor_dir}' not found.", file=sys.stderr)
        return
    available_hooks = set(get_available_templates())
    if not available_hooks:
        print("No available templates found.", file=sys.stderr)
        return
    # detect defined hook file and non template files
    defined_hooks_source, non_template_files = list_templates(
        visitor_dir, available_hooks
    )
    defined_hooks = {d["name"]: d["path"] for d in defined_hooks_source}
    defined_hooks = dict(sorted(defined_hooks.items()))
    non_template_files = sorted(list(non_template_files))
    print(
        f"Implemented hooks in '{visitor_dir}' (Implemented/Available: {len(defined_hooks)}/{len(available_hooks)}):"
    )
    print(
        "Note: Hooks are activated when the corresponding hook file exists. Otherwise, the default template is used."
    )
    adjusted_width = max((len(k) for k in defined_hooks.keys()), default=0) + 4
    for k, v in defined_hooks.items():
        print(f"  {k.ljust(adjusted_width)} {v}")
    if non_template_files:
        print(f"Non-template files in '{visitor_dir}':")
        for f in non_template_files:
            print(f"  {f}")


def parseInt(s: str) -> int:
    try:
        return int(s)
    except ValueError:
        return 0


def interactive_generate_single(mode: str):
    """Generate a single template."""
    tool_path = get_tool_path()
    if not tool_path:
        return
    # tool/ebmcodegen --mode hookkind
    command = [tool_path, "--mode", "hookkind"]
    try:
        result = subprocess.run(
            command, check=True, capture_output=True, text=True, encoding="utf-8"
        )
        output = json.loads(result.stdout)
        print("Which kind of template would you like to generate?")
        print("0. <Exit>")
        print(
            "\n".join(
                [f"{i + 1}. {prefix}" for i, prefix in enumerate(output["prefixes"])]
            )
        )
        choice = parseInt(input("Enter the number of your choice: ").strip())
        if choice == 0:
            print("Exiting the generation.")
            return
        index = choice - 1
        if index < 0 or index >= len(output["prefixes"]):
            print("Invalid choice. Exiting.")
            return
        selected_prefix = output["prefixes"][index]
        print(f"You selected: {selected_prefix}")
        available_templates = get_available_templates()
        suffixes = tuple([x for x in output["suffixes"] if x != "_dispatch"])
        matching_templates = [
            t
            for t in available_templates
            if t.startswith(selected_prefix) and not t.endswith(suffixes)
        ]
        if not matching_templates:
            print(f"No templates found for prefix '{selected_prefix}'. Exiting.")
            return
        matching_templates.sort()
        if len(matching_templates) > 1:
            print("Which specific template would you like to generate?")
            for i, template in enumerate(matching_templates):
                print(f"{i + 1}. {template}")
            choice = parseInt(input("Enter the number of your choice: ").strip())
            if choice == 0:
                print("Exiting the generation.")
                return
            index = choice - 1
            if index < 0 or index >= len(matching_templates):
                print("Invalid choice. Exiting.")
                return
            selected_template = matching_templates[index]
            print(f"You selected: {selected_template}")
        else:
            selected_template = matching_templates[0]
        matching_templates = [
            t
            for t in available_templates
            if t == selected_template or t.startswith(selected_template + "_")
        ]
        matching_templates.sort()
        print("Which template variant would you like to generate?")
        print("0. <Exit>")
        for i, template in enumerate(matching_templates):
            print(f"{i + 1}. {template}")
        choice = parseInt(input("Enter the number of your choice: ").strip())
        if choice == 0:
            print("Exiting the generation.")
            return
        index = choice - 1
        if index < 0 or index >= len(matching_templates):
            print("Invalid choice. Exiting.")
            return
        selected_variant = matching_templates[index]
        print(f"You selected: {selected_variant}")
        run_single_template(tool_path, selected_variant)
        would_save = (
            input(
                "Would you like to save this template to a language visitor directory? (y/N): "
            )
            .strip()
            .lower()
        )
        if would_save == "y":
            lang = input(
                "Enter the language directory to save to (e.g., 'cpp', 'python', or 'default'): "
            ).strip()
            if lang:
                run_save_template(tool_path, selected_variant, mode, lang)
    except subprocess.CalledProcessError as e:
        print(
            f"Error: Failed to get hooklist. ebmcodegen exited with code {e.returncode}",
            file=sys.stderr,
        )
        print(f"Stderr: {e.stderr}", file=sys.stderr)
        return


def interactive(mode: str):
    """interactive guide through the script features."""
    print("Welcome to the interactive guide!")
    print("This guide will help you use the ebmcodegen tool for template generation.")
    print("You can choose from the following options:")

    while True:
        command = input(
            "\nChoose an option:\n"
            "1. Generate a single template\n"
            "2. Update existing hook files\n"
            "3. Test generation for all templates\n"
            "4. List defined templates in a language directory\n"
            "5. Exit\n"
            "Enter the number of your choice: "
        ).strip()
        if command == "1":
            interactive_generate_single(mode)
        elif command == "2":
            lang = input(
                "Enter the language directory to update (e.g., 'cpp', 'python', or 'default'): "
            ).strip()
            if lang:
                tool_path = get_tool_path()
                if tool_path:
                    run_update_hooks(tool_path, lang, mode)
        elif command == "3":
            tool_path = get_tool_path()
            if tool_path:
                run_test_mode(tool_path)
        elif command == "4":
            lang = input(
                "Enter the language directory to list (e.g., 'cpp', 'python', or 'default'): "
            ).strip()
            if lang:
                list_defined_templates(lang, mode)
        elif command == "5":
            print("Exiting the interactive guide. Goodbye!")
            break
        else:
            print("Invalid choice. Please enter a number from 1 to 5.")


def use_class_default(target: str) -> str:
    replace_target = [
        "Statement",
        "Expression",
        "Type",
        "Block",
        "entry",
        "post_entry",
        "pre_visitor",
    ]
    isClassTarget = any([target.startswith(r) for r in replace_target])
    if isClassTarget and not target.endswith("_class") and not target.endswith("_old"):
        target += "_class"
    return target.removesuffix("_old")


def main(mode: str):
    if mode != "codegen" and mode != "interpret":
        print(
            f"Error: Unsupported MODE '{mode}'. Only 'codegen' and 'interpret' are supported."
        )
        sys.exit(1)
    # Ensure the script is run from the project root
    if not os.path.exists("tool"):
        print(
            "Error: This script must be run from the root of the rebrgen project.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Check for the required argument
    if len(sys.argv) < 2:
        print(
            f"Usage: python {sys.argv[0]} <template_target | test | update> [lang]",
            file=sys.stderr,
        )
        print(
            "  <template_target>: Generate a template to stdout (e.g., 'Statement_BLOCK').",
            file=sys.stderr,
        )
        print(
            "  interactive      : Start an interactive guide through the script features.",
            file=sys.stderr,
        )
        print(
            "  test             : Test generation for all available templates.",
            file=sys.stderr,
        )
        print(
            "  update           : Update all hook files in the specified [lang] directory.",
            file=sys.stderr,
        )
        print(
            "  list             : List all defined templates in the specified [lang] directory.",
            file=sys.stderr,
        )
        print(
            "  [lang]           : Optional/Required. For new templates, saves to 'src/ebmcg/ebm2<lang>/visitor/'. Required for 'update'.",
            file=sys.stderr,
        )
        print(
            "\nTo see available targets, run './tool/ebmcodegen --mode hooklist'",
            file=sys.stderr,
        )
        sys.exit(1)

    tool_path = get_tool_path()
    if not tool_path:
        sys.exit(1)

    target_arg = sys.argv[1]

    if target_arg == "interactive":
        interactive(mode)
        return
    if target_arg == "test":
        run_test_mode(tool_path)
    elif target_arg == "update":
        if len(sys.argv) < 3:
            print(
                "Error: 'update' command requires a [lang] argument.", file=sys.stderr
            )
            sys.exit(1)
        lang_arg = sys.argv[2]
        run_update_hooks(tool_path, lang_arg, mode)
    elif target_arg == "list":
        if len(sys.argv) < 3:
            print("Error: 'list' command requires a [lang] argument.", file=sys.stderr)
            sys.exit(1)
        lang_arg = sys.argv[2]
        list_defined_templates(lang_arg, mode)
    elif len(sys.argv) == 3:
        lang_arg = sys.argv[2]
        target_arg = use_class_default(target_arg)
        run_save_template(tool_path, target_arg, mode, lang_arg)
    else:
        target_arg = use_class_default(target_arg)
        run_single_template(tool_path, target_arg)


if __name__ == "__main__":
    main("codegen")
