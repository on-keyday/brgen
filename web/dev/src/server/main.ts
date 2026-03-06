import { serve } from "@hono/node-server";
import { createApp, createGeneratorService,patchWebWorker } from "./core.js";
import { installNodePolyfills } from "./node_compat.js";

installNodePolyfills();
patchWebWorker()

const port = parseInt(process.env.PORT ?? "8080", 10);

async function main() {
    console.log("Initializing generator service...");
    const service = await createGeneratorService();
    const app = createApp(service);

    serve({ fetch: app.fetch, port }, (info) => {
        console.log(`Server is running on http://localhost:${info.port}`);
        console.log(`  REST API : http://localhost:${info.port}/api/generate`);
        console.log(`  MCP HTTP : http://localhost:${info.port}/mcp`);
    });
}

main().catch((err) => {
    console.error("Failed to start server:", err);
    process.exit(1);
});
