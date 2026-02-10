import { defineConfig } from "vite";
import preact from "@preact/preset-vite";
import path from "path";

export default defineConfig({
  plugins: [preact()],
  root: ".",
  publicDir: false,
  build: {
    outDir: "dist",
    emptyOutDir: true,
    commonjsOptions: {
      // ast2ts is CJS (symlinked from astlib/ast2ts/out/ with no package.json).
      // Include it in CJS→ESM conversion so named exports work.
      include: [/ast2ts/, /node_modules/],
      transformMixedEsModules: true,
    },
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
      // ast2ts is a symlink to astlib/ast2ts/out/ which has no package.json,
      // so Vite can't resolve it as a package. Map to the CJS entry directly.
      "ast2ts": path.resolve(__dirname, "node_modules/ast2ts/index.js"),
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
