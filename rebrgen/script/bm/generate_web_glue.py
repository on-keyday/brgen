import subprocess as sp
import os
import json


def search_config_files(root_dir):
    config_files = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "config.json":
                config_files.append(os.path.join(root, file))
    return config_files


def generate_web_glue(config_file):
    hook_dir = os.path.join(os.path.dirname(config_file), "hook")
    CONFIG = dict[str, str](
        json.loads(
            sp.check_output(
                ["tool/gen_template", "--config-file", config_file, "--mode", "config"]
            )
        )
    )
    LANG_NAME = str(
        CONFIG["worker_request_name"]
        if CONFIG["worker_request_name"] != ""
        else CONFIG["lang"]
    )
    LSP_LANG = str(
        CONFIG["worker_lsp_name"] if CONFIG["worker_lsp_name"] != "" else CONFIG["lang"]
    )
    print(f"Generating web glue for {LANG_NAME} ({config_file})")
    UPPER_LANG_NAME = LANG_NAME[0].upper() + LANG_NAME[1:]
    UI_CODE = sp.check_output(
        [
            "tool/gen_template",
            "--config-file",
            config_file,
            "--hook-dir",
            hook_dir,
            "--mode",
            "js-ui-embed",
        ]
    )
    WORKER_CODE = sp.check_output(
        [
            "tool/gen_template",
            "--config-file",
            config_file,
            "--hook-dir",
            hook_dir,
            "--mode",
            "js-worker",
        ]
    )
    CALL_WORKER_FUNC = "generate" + UPPER_LANG_NAME
    CALL_UI_FUNC = "set" + UPPER_LANG_NAME + "UIConfig"
    CALL_UI_TO_OPT_FUNC = "convert" + UPPER_LANG_NAME + "UIConfigToOption"
    CALL_SET_UI_FUNC = "set" + UPPER_LANG_NAME + "UIConfig"
    return {
        "lang_name": LANG_NAME,
        "worker_name": "bm2" + CONFIG["lang"],
        "ui_code": UI_CODE,
        "worker_code": WORKER_CODE,
        "call_worker_func": CALL_WORKER_FUNC,
        "call_ui_func": CALL_UI_FUNC,
        "call_ui_to_opt_func": CALL_UI_TO_OPT_FUNC,
        "call_set_ui_func": CALL_SET_UI_FUNC,
        "lsp_lang": LSP_LANG,
    }


def generate_web_glue_files(config_dir, output_dir):
    config_files = search_config_files(config_dir)
    UI_GLUE = b""
    UI_CALLS = b""
    UI_SETS = b""
    LSP_MAPPER = b"export const BM_LSP_LANGUAGES = Object.freeze({\n"
    UI_CANDIDATES = b"export const BM_LANGUAGES = Object.freeze(["
    BROWSER_WORKER_FACTORY = b"export const bm_workers = Object.freeze({\n"
    COPY_WASM = b""
    for config_file in config_files:
        web_glue = generate_web_glue(config_file)
        with open(f"{output_dir}/{web_glue['worker_name']}_worker.js", "wb") as f:
            f.write(web_glue["worker_code"])
        UI_GLUE += web_glue["ui_code"]
        UI_CALLS += f"    case \"{web_glue['lang_name']}\": return {web_glue['call_worker_func']}(factory,traceID,{web_glue["call_ui_to_opt_func"]}(ui),sourceCode);\n".encode()
        UI_CANDIDATES += f"\"{web_glue['lang_name']}\", ".encode()
        BROWSER_WORKER_FACTORY += f"    \"{web_glue['worker_name']}\": () => new Worker(new URL('./{web_glue['worker_name']}_worker.js', import.meta.url),{"{type: \"module\"}"}),\n".encode()
        UI_SETS += f"  {web_glue['call_set_ui_func']}(ui);\n".encode()
        LSP_MAPPER += (
            f"  \"{web_glue['lang_name']}\": \"{web_glue['lsp_lang']}\",\n".encode()
        )
        COPY_WASM += f"copyWasm('bmgen/{web_glue['worker_name']}.js');\n".encode()
        COPY_WASM += f"copyWasm('bmgen/{web_glue['worker_name']}.wasm');\n".encode()
        COPY_WASM += (
            f"copyWasm('bmgen/{web_glue['worker_name']}_worker.js');\n".encode()
        )  # also copy worker js
    UI_CANDIDATES += b"]);\n"
    BROWSER_WORKER_FACTORY += b"});\n"
    LSP_MAPPER += b"});\n"
    with open(f"{output_dir}/bm_caller.js", "wb") as f:
        f.write(UI_GLUE)
        f.write(
            b"export function generateBMCode(factory,ui,traceID,lang,sourceCode) {"
            + b" switch(lang) {\n"
            + UI_CALLS
            + b"    default: throw new Error('Unsupported language: ' + lang);\n"
            + b"  }\n"
            + b"}\n"
        )
        f.write(UI_CANDIDATES)
        f.write(LSP_MAPPER)
        f.write(b"export function setBMUIConfig(ui) {\n" + UI_SETS + b"}\n")

    with open(f"{output_dir}/bm_workers.js", "wb") as f:
        f.write(BROWSER_WORKER_FACTORY)

    with open(f"{output_dir}/wasmCopy.js.txt", "wb") as f:
        f.write(COPY_WASM)


if __name__ == "__main__":
    generate_web_glue_files("src", "web/tool")
