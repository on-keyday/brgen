import { serve } from "@hono/node-server";
import { createApp, createGeneratorService } from "./core.js";

const port = parseInt(process.env.PORT ?? "8080", 10);

async function main() {
    console.log("Initializing generator service...");
    const service = await createGeneratorService();
    const app = createApp(service);

    serve({ fetch: app.fetch, port }, (info) => {
        console.log(`Server is running on http://localhost:${info.port}`);
    });
}

main().catch((err) => {
    console.error("Failed to start server:", err);
    process.exit(1);
});
