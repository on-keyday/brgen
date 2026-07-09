#!/usr/bin/env python3
# Test logic for ebm2ts (TypeScript / JavaScript). Runs a round-trip
# decode -> re-encode against the generated module via Node.js (which
# supports both .ts and .js natively).
import json
import os
import pathlib as pl
import subprocess as sp
import sys

import unictest_report


WRITE_BUFFER_BYTES = 16 * 1024 * 1024  # 16 MiB; large enough for wasm_src2json + EBM self-test


def main():
    TEST_TARGET_FILE = sys.argv[1]
    INPUT_FILE = sys.argv[2]
    OUTPUT_FILE = sys.argv[3]
    TEST_TARGET_FORMAT = sys.argv[4]
    OPTION_SET_NAME = sys.argv[5]
    ADDITIONAL_ARGS = sys.argv[6:] if len(sys.argv) > 6 else []

    is_javascript = OPTION_SET_NAME == "javascript" or "--javascript" in ADDITIONAL_ARGS
    suffix = ".js" if is_javascript else ".ts"

    work_dir = pl.Path(os.getcwd())
    module_path = work_dir / f"target_module{suffix}"
    driver_path = work_dir / "test_driver.ts"  # node strips types from .ts directly

    # Stage the generated module next to the driver, with the right extension.
    with open(TEST_TARGET_FILE, "rb") as src, open(module_path, "wb") as dst:
        dst.write(src.read())

    decode_fn = f"{TEST_TARGET_FORMAT}_decode"
    encode_fn = f"{TEST_TARGET_FORMAT}_encode"
    # Node ESM module loader keys on the file path. Use an explicit URL to
    # bypass any package.json type confusion.
    module_url_expr = (
        f"new URL('./target_module{suffix}', import.meta.url).href"
    )

    # Avoid calling `process.exit()` after a sync console.error: on Windows
    # node 25, the immediate event-loop abort races against the still-pending
    # async write to stderr inside libuv (Assertion failed: UV_HANDLE_CLOSING
    # in src/win/async.c). We use distinct error types + `process.exitCode`
    # and let Node drain its own I/O before exiting naturally.
    driver_src = f"""// Auto-generated round-trip driver for {TEST_TARGET_FORMAT}.
import {{ readFileSync, writeFileSync }} from 'node:fs';

class DecodeError extends Error {{}}
class EncodeError extends Error {{}}

async function main() {{
    const inputPath = process.argv[2];
    const outputPath = process.argv[3];

    const buf = readFileSync(inputPath);
    const inputBytes = new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);

    const mod = await import({module_url_expr});
    const decode = mod[{json.dumps(decode_fn)}];
    const encode = mod[{json.dumps(encode_fn)}];
    if (typeof decode !== 'function' || typeof encode !== 'function') {{
        throw new Error('Missing {decode_fn} or {encode_fn} export');
    }}

    const reader = {{
        view: new DataView(inputBytes.buffer, inputBytes.byteOffset, inputBytes.byteLength),
        offset: 0,
    }};

    let obj;
    try {{
        obj = decode(reader);
    }} catch (e) {{
        throw new DecodeError(`Decode error: ${{e && e.stack ? e.stack : e}}`);
    }}

    const writeBuf = new ArrayBuffer({WRITE_BUFFER_BYTES});
    const writer = {{
        view: new DataView(writeBuf),
        offset: 0,
    }};

    try {{
        encode(writer, obj);
    }} catch (e) {{
        throw new EncodeError(`Encode error: ${{e && e.stack ? e.stack : e}}`);
    }}

    const out = new Uint8Array(writeBuf, 0, writer.offset);
    writeFileSync(outputPath, out);
    console.log('Round-trip succeeded, wrote', writer.offset, 'bytes');
}}

main().catch((e) => {{
    // `process.stderr.write` with a callback drains libuv's TTY/Pipe write
    // queue before we surface the exit code; `process.exit` would race the
    // pending write on Windows (UV_HANDLE_CLOSING assertion).
    process.stderr.write(`${{e && e.message ? e.message : e}}\\n`, () => {{
        process.exitCode = (e instanceof DecodeError) ? 10
            : (e instanceof EncodeError) ? 20
            : 1;
    }});
}});
"""

    with open(driver_path, "w", encoding="utf-8") as f:
        f.write(driver_src)

    print(
        f"Testing ebm2ts ({OPTION_SET_NAME}): format={TEST_TARGET_FORMAT}, "
        f"module={module_path.name}, driver={driver_path.name}",
        flush=True,
    )

    cmd = ["node", str(driver_path), INPUT_FILE, OUTPUT_FILE]
    print(f"Running: {' '.join(cmd)}", flush=True)
    print("for VSCode debugging")
    print(
        json.dumps(
            {
                "type": "node",
                "request": "launch",
                "cwd": os.getcwd(),
                "name": f"Debug ebm2ts unictest ({TEST_TARGET_FORMAT}, {OPTION_SET_NAME})",
                "runtimeExecutable": "node",
                "program": str(driver_path),
                "args": [INPUT_FILE, OUTPUT_FILE],
            },
            indent=4,
        )
    )

    # Capture (rather than pass through) so we can surface the ts-harness
    # diagnostic (Decode/Encode error, or a tsc TS#### line) as the failure
    # reason; the full log is still re-emitted for the artifact.
    proc = sp.run(cmd, env=os.environ, capture_output=True, text=True)
    sys.stdout.write(proc.stdout or "")
    sys.stderr.write(proc.stderr or "")
    if proc.returncode != 0:
        # The driver exits 10 on DecodeError, 20 on EncodeError (stderr carries
        # "Decode error: ..." etc.); tsc/other failures fall through to 'run'.
        phase = {10: "decode", 20: "encode"}.get(proc.returncode, "run")
        unictest_report.fail(phase, proc.stderr or proc.stdout, code=proc.returncode)


if __name__ == "__main__":
    main()
