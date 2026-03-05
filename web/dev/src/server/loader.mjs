// loader.mjs
import { isBuiltin } from "node:module";
import { dirname, resolve as pathResolve } from "node:path"; // 名前を衝突させない
import { existsSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";

export async function resolve(specifier, context, nextResolve) {
  const { parentURL } = context;

  // 内蔵モジュールや親URLがない場合はそのまま次に渡す
  if (!parentURL || isBuiltin(specifier)) {
    return nextResolve(specifier, context);
  }

  // 相対パス（./ か ../）かつ .js で終わるものだけを対象にする
  if (
    specifier.endsWith(".js") &&
    (specifier.startsWith("./") || specifier.startsWith("../"))
  ) {
    try {
      const parentPath = fileURLToPath(parentURL);
      const parentDir = dirname(parentPath);
      // インポートしようとしているファイルの絶対パスを計算（拡張子を.tsに差し替え）
      const potentialTsPath = pathResolve(
        parentDir,
        specifier.replace(/\.js$/, ".ts"),
      );

      // 実際に .ts ファイルが存在するか確認
      if (existsSync(potentialTsPath)) {
        // 存在すれば、その .ts ファイルの URL を返す
        const newUrl = pathToFileURL(potentialTsPath).href;
        return nextResolve(newUrl, context);
      }
    } catch (e) {
      // 解析に失敗した場合は無視して標準の解決に任せる
    }
  }

  return nextResolve(specifier, context);
}
