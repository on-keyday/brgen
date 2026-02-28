import { IWorker } from "../s2j/job_mgr.js";
import { JobRequest, RequestLanguage, WorkerType } from "../s2j/msg.js";
import { MyEmscriptenModule } from "../s2j/emscripten_mod.js";
import {
    DirectWorker,
    createEmscriptenHandler,
    createGoHandler,
    createRustWasmHandler,
} from "./direct_worker.js";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

type WorkerMap = Readonly<{ [key: string]: () => IWorker }>;

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const libDir = resolve(__dirname, "../lib");

// --- Request callbacks (same logic as browser workers) ---

const src2jsonRequestCallback = async (e: JobRequest, _m: MyEmscriptenModule) => {
    // Note: fetchImportFile is skipped in server mode (no fetch API for examples)
    switch (e.lang) {
        case RequestLanguage.JSON_AST:
            return ["src2json", "--argv", e.sourceCode, "--no-color", "--print-json", "--print-on-error"];
        case RequestLanguage.JSON_DEBUG_AST:
            return ["src2json", "--argv", e.sourceCode, "--no-color", "--print-json", "--print-on-error", "--debug-json"];
        case RequestLanguage.TOKENIZE:
            return ["src2json", "--argv", e.sourceCode, "--no-color", "--print-json", "--print-on-error", "--lexer"];
        default:
            return new Error("unknown message type");
    }
};

const json2cpp2RequestCallback = (e: JobRequest, m: MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.CPP:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2cpp2", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
};

const json2cRequestCallback = (e: JobRequest, m: MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.C:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2c", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
};

const json2tsRequestCallback = (e: JobRequest, m: MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.TYPESCRIPT:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["json2ts", "/editor.json", "--no-color"];
        default:
            return new Error("unknown message type");
    }
};

const bmgenRequestCallback = (e: JobRequest, m: MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.BINARY_MODULE:
            m.FS.writeFile("/editor.json", e.sourceCode);
            if (e.arguments?.includes("--print-instructions")) {
                return ["bmgen", "-i", "/editor.json"];
            } else {
                return ["bmgen", "-i", "/editor.json", "--base64", "-o", "-"];
            }
        default:
            return new Error("unknown message type");
    }
};

const ebmgenRequestCallback = (e: JobRequest, m: MyEmscriptenModule) => {
    switch (e.lang) {
        case RequestLanguage.EBM:
            m.FS.writeFile("/editor.json", e.sourceCode);
            return ["ebmgen", "-i", "/editor.json"];
        default:
            return new Error("unknown message type");
    }
};

const json2goRequestCallback = (e: JobRequest) => {
    switch (e.lang) {
        case RequestLanguage.GO:
            return e.sourceCode;
        default:
            return new Error("unknown message type");
    }
};

const json2kaitaiRequestCallback = (e: JobRequest) => {
    switch (e.lang) {
        case RequestLanguage.KAITAI_STRUCT:
            return e.sourceCode;
        default:
            return new Error("unknown message type");
    }
};

// --- Helper: load Emscripten factory from a .js file ---

function emscriptenFactoryFromImport(
    importPromise: Promise<any>,
): Promise<EmscriptenModuleFactory<MyEmscriptenModule>> {
    return importPromise.then((m) => m.default as EmscriptenModuleFactory<MyEmscriptenModule>);
}

// --- Core Worker Map ---

export const directFixedWorkerMap: WorkerMap = Object.freeze({
    [WorkerType.SRC2JSON]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/src2json.js")),
                src2jsonRequestCallback,
            ),
        ),
    [WorkerType.JSON2CPP2]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/json2cpp2.js")),
                json2cpp2RequestCallback,
            ),
        ),
    [WorkerType.JSON2GO]: () =>
        new DirectWorker(
            createGoHandler(
                resolve(libDir, "json2go.wasm"),
                json2goRequestCallback,
                "json2go",
            ),
        ),
    [WorkerType.JSON2C]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/json2c.js")),
                json2cRequestCallback,
            ),
        ),
    [WorkerType.JSON2RUST]: () => new DirectWorker(createRustWasmHandler()),
    [WorkerType.JSON2TS]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/json2ts.js")),
                json2tsRequestCallback,
            ),
        ),
    [WorkerType.JSON2KAITAI]: () =>
        new DirectWorker(
            createGoHandler(
                resolve(libDir, "json2kaitai.wasm"),
                json2kaitaiRequestCallback,
                "json2kaitai",
            ),
        ),
    [WorkerType.BMGEN]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/bmgen/bmgen.js")),
                bmgenRequestCallback,
            ),
        ),
    [WorkerType.EBMGEN]: () =>
        new DirectWorker(
            createEmscriptenHandler(
                emscriptenFactoryFromImport(import("../lib/bmgen/ebmgen.js")),
                ebmgenRequestCallback,
            ),
        ),
});

// --- Base64 decoding (same as auto-generated worker code) ---

function base64ToUint8Array(base64: string): Uint8Array | Error {
    if (typeof (Uint8Array as any).fromBase64 === "function") {
        return (Uint8Array as any).fromBase64(base64);
    }
    const base64Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let cleanedBase64 = base64.replace(/-/g, "+").replace(/_/g, "/").trim();
    const padding = (4 - (cleanedBase64.length % 4)) % 4;
    cleanedBase64 += "=".repeat(padding);
    const rawLength = cleanedBase64.length;
    const decodedLength = (rawLength * 3) / 4 - padding;
    const uint8Array = new Uint8Array(decodedLength);
    let byteIndex = 0;
    for (let i = 0; i < rawLength; i += 4) {
        const encoded1 = base64Characters.indexOf(cleanedBase64[i]);
        const encoded2 = base64Characters.indexOf(cleanedBase64[i + 1]);
        const encoded3 = base64Characters.indexOf(cleanedBase64[i + 2]);
        const encoded4 = base64Characters.indexOf(cleanedBase64[i + 3]);
        if (encoded1 === -1 || encoded2 === -1) {
            return new Error("invalid base64 string");
        }
        const decoded1 = (encoded1 << 2) | (encoded2 >> 4);
        const decoded2 = ((encoded2 & 15) << 4) | (encoded3 >> 2);
        const decoded3 = ((encoded3 & 3) << 6) | encoded4;
        uint8Array[byteIndex++] = decoded1;
        if (encoded3 !== -1) uint8Array[byteIndex++] = decoded2;
        if (encoded4 !== -1) uint8Array[byteIndex++] = decoded3;
    }
    return uint8Array.subarray(0, byteIndex);
}

// --- Generic BM request callback ---

function createBmRequestCallback(toolName: string) {
    return (e: JobRequest, m: MyEmscriptenModule): string[] | Error => {
        const bm = base64ToUint8Array(e.sourceCode);
        if (bm instanceof Error) return bm;
        m.FS.writeFile("/editor.bm", bm);
        return [toolName, "-i", "/editor.bm"];
    };
}

// --- Generic EBM request callback ---

function createEbmRequestCallback(toolName: string) {
    return (e: JobRequest, m: MyEmscriptenModule): string[] | Error => {
        const bm = base64ToUint8Array(e.sourceCode.trim());
        if (bm instanceof Error) return bm;
        m.FS.writeFile("/editor.ebm", bm);
        return [toolName, "-i", "/editor.ebm"];
    };
}

// --- BM Worker Map (dynamic, from bm_workers.js keys) ---

export async function createDirectBmWorkerMap(): Promise<WorkerMap> {
    const { bm_workers } = await import("../lib/bmgen/bm_workers.js");
    const map: { [key: string]: () => IWorker } = {};

    for (const key of Object.keys(bm_workers)) {
        // key is like "bm2kaitai", "bm2go", etc.
        // The corresponding Emscripten module is at ../lib/bmgen/<key>.js
        const toolName = key; // e.g., "bm2kaitai"
        map[key] = () =>
            new DirectWorker(
                createEmscriptenHandler(
                    emscriptenFactoryFromImport(
                        import(`../lib/bmgen/${toolName}.js`),
                    ),
                    createBmRequestCallback(toolName),
                ),
            );
    }

    return Object.freeze(map);
}

// --- EBM Worker Map (dynamic, from ebm_workers.js keys) ---

export async function createDirectEbmWorkerMap(): Promise<WorkerMap> {
    const { ebm_workers } = await import("../lib/bmgen/ebm_workers.js");
    const map: { [key: string]: () => IWorker } = {};

    for (const key of Object.keys(ebm_workers)) {
        // key is like "ebm2json", "ebm2c", etc.
        const toolName = key;
        map[key] = () =>
            new DirectWorker(
                createEmscriptenHandler(
                    emscriptenFactoryFromImport(
                        import(`../lib/bmgen/${toolName}.js`),
                    ),
                    createEbmRequestCallback(toolName),
                ),
            );
    }

    return Object.freeze(map);
}
