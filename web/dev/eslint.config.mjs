import js from "@eslint/js";
import tseslint from "typescript-eslint";

export default tseslint.config(
  js.configs.recommended,
  ...tseslint.configs.recommended,
  {
    rules: {
      // import type の使用を強制する設定
      "@typescript-eslint/consistent-type-imports": [
        "error",
        {
          prefer: "type-imports", // 型は 'import type' を使う
          fixStyle: "separate-type-imports", // 既存の import と混ぜず独立させる
        },
      ],
      // 逆に、値として使っているものを 'import type' で書くとエラーにする（整合性のため）
      "@typescript-eslint/no-import-type-side-effects": "error",
    },
  },
);
