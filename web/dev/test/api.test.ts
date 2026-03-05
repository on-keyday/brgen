/**
 * Integration tests for the brgen API server (REST + MCP).
 *
 * Uses Hono's built-in app.request() to test endpoints in-process,
 * so no real HTTP server is needed.  The first code-generation request
 * triggers lazy WASM loading (~5-20 s), which is why testTimeout is 60 s.
 */
import { describe, it, expect, beforeAll } from "vitest";
import type { Hono } from "hono";
import { installNodePolyfills } from "../src/server/node_compat.js";
import { createGeneratorService, createApp } from "../src/server/core.js";

// Apply Node.js polyfills (WorkerGlobalScope, file:// fetch) before WASM loads.
installNodePolyfills();

let app: Hono;
beforeAll(async () => {
    const service = await createGeneratorService();
    app = createApp(service);
});

// ---------------------------------------------------------------------------
// REST: GET /api/languages
// ---------------------------------------------------------------------------
describe("GET /api/languages", () => {
    it("returns a non-empty array", async () => {
        const res = await app.request("/api/languages");
        expect(res.status).toBe(200);
        const langs = await res.json() as any[];
        expect(langs.length).toBeGreaterThan(0);
    });

    it("each entry has id, displayName, category, options", async () => {
        const res = await app.request("/api/languages");
        const langs = await res.json() as any[];
        for (const l of langs) {
            expect(typeof l.id).toBe("string");
            expect(typeof l.displayName).toBe("string");
            expect(typeof l.category).toBe("string");
            expect(Array.isArray(l.options)).toBe(true);
        }
    });

    it("cpp has expected options", async () => {
        const res = await app.request("/api/languages");
        const langs = await res.json() as any[];
        const cpp = langs.find((l: any) => l.id === "cpp");
        expect(cpp).toBeDefined();
        const keys = cpp.options.map((o: any) => o.key);
        expect(keys).toContain("use_error");
        expect(keys).toContain("enum_stringer");
    });
});

// ---------------------------------------------------------------------------
// REST: POST /api/generate
// ---------------------------------------------------------------------------
describe("POST /api/generate", () => {
    const BGN_SOURCE = "format A:\n  a :u8\n";

    const generate = async(body: object) =>
        app.request("/api/generate", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(body),
        });

    it("generates C++ code", async () => {
        const res = await generate({ source: BGN_SOURCE, lang: "cpp" });
        expect(res.status).toBe(200);
        const result = await res.json() as any;
        expect(result.isError).toBe(false);
        expect(result.code).toContain("struct A");
    });

    it("generation options are respected (use_error)", async () => {
        const [r1, r2] = await Promise.all([
            generate({ source: BGN_SOURCE, lang: "cpp", options: { use_error: true } }).then(r => r.json()),
            generate({ source: BGN_SOURCE, lang: "cpp", options: { use_error: false } }).then(r => r.json()),
        ]) as [any, any];
        expect(r1.isError).toBe(false);
        expect(r2.isError).toBe(false);
        // use_error adds error-handling headers to the generated code
        expect(r1.code).toContain("error");
        expect(r2.code).not.toContain("futils::error::Error");
    });

    it("returns 400 for unsupported language", async () => {
        const res = await generate({ source: BGN_SOURCE, lang: "cobol" });
        expect(res.status).toBe(400);
    });

    it("returns 400 when source or lang is missing", async () => {
        expect((await generate({ lang: "cpp" })).status).toBe(400);
        expect((await generate({ source: BGN_SOURCE })).status).toBe(400);
    });
});

// ---------------------------------------------------------------------------
// MCP: POST /mcp
// ---------------------------------------------------------------------------
describe("MCP /mcp", () => {
    const BGN_SOURCE = "format A:\n  a :u8\n";

    const mcp = (body: object) =>
        app.request("/mcp", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
                "Accept": "application/json, text/event-stream",
            },
            body: JSON.stringify(body),
        });

    it("tools/list returns generate_code and list_languages", async () => {
        const res = await mcp({ jsonrpc: "2.0", id: 1, method: "tools/list", params: {} });
        expect(res.status).toBe(200);
        const data = await res.json() as any;
        const names = data.result.tools.map((t: any) => t.name);
        expect(names).toContain("generate_code");
        expect(names).toContain("list_languages");
    });

    it("list_languages includes option metadata", async () => {
        const res = await mcp({
            jsonrpc: "2.0", id: 2,
            method: "tools/call",
            params: { name: "list_languages", arguments: {} },
        });
        expect(res.status).toBe(200);
        const data = await res.json() as any;
        expect(data.result.isError).toBeFalsy();
        const text: string = data.result.content[0].text;
        expect(text).toContain("cpp");
        expect(text).toContain("use_error");
    });

    it("generate_code generates C++", async () => {
        const res = await mcp({
            jsonrpc: "2.0", id: 3,
            method: "tools/call",
            params: { name: "generate_code", arguments: { source: BGN_SOURCE, lang: "cpp" } },
        });
        expect(res.status).toBe(200);
        const data = await res.json() as any;
        expect(data.result.isError).toBeFalsy();
        expect(data.result.content[0].text).toContain("struct A");
    });

    it("generate_code returns isError for unknown language", async () => {
        const res = await mcp({
            jsonrpc: "2.0", id: 4,
            method: "tools/call",
            params: { name: "generate_code", arguments: { source: BGN_SOURCE, lang: "cobol" } },
        });
        expect(res.status).toBe(200);
        const data = await res.json() as any;
        expect(data.result.isError).toBe(true);
    });
});
