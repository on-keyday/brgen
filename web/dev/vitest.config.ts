import { defineConfig, mergeConfig } from "vitest/config";
import viteConfig from "./vite.config"; // 既存の vite 設定をインポート

export default mergeConfig(
  viteConfig,
  defineConfig({
    test: {
      environment: "node",
      pool: "forks",
      poolOptions: {
        forks: {
            execArgv: [
            '--experimental-strip-types',
            '--experimental-transform-types',
            '--import', './src/server/register-loader.mjs'
            ]
        },
      },
      testTimeout: 60_000,
      // deps.optimizer.web.include ではなく、server.deps.inline を使用して
      // シンボリックリンク先の CJS ファイルを Vite/Vitest の変換対象に含める
      server: {
        deps: {
          inline: [/ast2ts/],
        },
      },
      
    },
    // Vitest (Node.js環境) において shims が不要な場合は alias を上書き、
    // 必要であれば viteConfig のものをそのまま継承します。
  })
);