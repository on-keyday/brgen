/**
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
import { createGeneratorService } from "./core.js";
import { createMcpServer } from "./mcp.js";
import { installNodePolyfills } from "./node_compat.js";

installNodePolyfills();

async function main() {
    const service = await createGeneratorService();
    const server = createMcpServer(service);
    const transport = new StdioServerTransport();
    await server.connect(transport);
}

main().catch((err) => {
    console.error("MCP server error:", err);
    process.exit(1);
});
