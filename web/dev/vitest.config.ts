import { defineConfig } from "vitest/config";

export default defineConfig({
    test: {
        environment: "node",
        pool: "forks",      // isolate each test file in its own process
        testTimeout: 60_000, // first request triggers WASM loading (~5-20s)
    },
});
