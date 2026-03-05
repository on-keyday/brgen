/**
 * Node.js compatibility polyfills for running Emscripten WASM modules
 * that were compiled with ENVIRONMENT=worker.
 *
 * Must be called once at process startup, before any Emscripten factory is invoked.
 */
export function installNodePolyfills(): void {
    // 1. WorkerGlobalScope
    //    Emscripten worker-environment modules assert typeof WorkerGlobalScope !== "undefined".
    if (typeof (globalThis as any).WorkerGlobalScope === "undefined") {
        (globalThis as any).WorkerGlobalScope = {};
    }

    // 2. fetch() for file:// URLs
    //    Emscripten loads the .wasm binary via fetch(new URL('foo.wasm', import.meta.url).href)
    //    which resolves to a file:// URL. Node.js 18+ fetch does not support file:// URLs,
    //    so we intercept them and serve the file using node:fs.
    const originalFetch = globalThis.fetch.bind(globalThis);
    globalThis.fetch = async function patchedFetch(
        input: RequestInfo | URL,
        init?: RequestInit,
    ): Promise<Response> {
        const url =
            typeof input === "string"
                ? input
                : input instanceof URL
                  ? input.href
                  : (input as Request).url;

        if (url.startsWith("file://")) {
            const { readFile } = await import("node:fs/promises");
            const { fileURLToPath } = await import("node:url");
            const data = await readFile(fileURLToPath(url));
            return new Response(data, {
                status: 200,
                headers: { "Content-Type": "application/wasm" },
            });
        }

        return originalFetch(input as Parameters<typeof originalFetch>[0], init);
    } as typeof globalThis.fetch;
}
