/**
 * CURRENTLY, THIS IS NOT WORKING DUE TO ISSUES WITH WEB WORKER LOGGING AND POLYFILLS.
 * Standalone MCP server entry point using stdio transport.
 *
 * Designed for integration with Claude Desktop, Claude Code, and other
 * MCP-compatible AI clients.
 *
 * Usage in claude_desktop_config.json:
 * {
 *   "mcpServers": {
 *     "brgen": {
 *       "command": "npx",
 *       "args": ["tsx", "/path/to/web/dev/src/server/mcp_main.ts"]
 *     }
 *   }
 * }
 *
 * Or via npm:
 *   cd web/dev && npm run mcp
 */
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { createGeneratorService, patchWebWorker } from "./core.js";
import { createMcpServer } from "./mcp.js";
import { installNodePolyfills } from "./node_compat.js";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";

installNodePolyfills();
let serverGlobal : McpServer | null = null;
const safeJsonStringify = (obj: any): string => {
    try {
        return JSON.stringify(obj);
    } catch (err) {
        return `<<Unserializable: ${err}>>`;
    }
};

patchWebWorker((level, ...args) => {
    if(serverGlobal) {
        serverGlobal.sendLoggingMessage({
            level: level as any,
            data: args.map(arg => (typeof arg === "string" ? arg :safeJsonStringify(arg) )),
        })
    }
}); // hook Web Worker to capture logs

async function main() {
    const service = await createGeneratorService();
    const server = createMcpServer(service,(...args) => {
        // avoid console.log
        server.sendLoggingMessage({
            level: "debug",
            data: args.map(arg => (typeof arg === "string" ? arg :safeJsonStringify(arg) )),
        })
        console.warn("Log from MCP server:", ...args);
    });
    serverGlobal = server; // set global reference for log capturing
    const transport = new StdioServerTransport();
    await server.connect(transport);
}

main().catch((err) => {
    console.error("MCP server error:", err);
    process.exit(1);
});
