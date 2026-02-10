import { defineConfig } from "vite";
import preact from "@preact/preset-vite";

export default defineConfig({
  plugins: [preact()],
  root: ".",
  publicDir: false,
  build: {
    outDir: "dist",
    emptyOutDir: true,
    rollupOptions: {
      input: "index.html",
      output: {
        entryFileNames: "script/index.js",
      },
    },
  },
  resolve: {
    alias: {
      "node:module": "/src/shims/empty.ts",
      "node:fs": "/src/shims/empty.ts",
      "node:path": "/src/shims/empty.ts",
    },
  },
  server: {
    headers: {
      // Required for SharedArrayBuffer (WASM workers)
      "Cross-Origin-Opener-Policy": "same-origin",
      "Cross-Origin-Embedder-Policy": "require-corp",
    },
  },
  worker: {
    format: "es",
  },
  assetsInclude: ["**/*.wasm"],
});
