# EBM の interactive query エンジンの開発

## 日付

2026-04-15

## 判断

ebmgen に interactive query エンジン（ebmgen --interactive）を実装し、
EBM のグラフ構造をクエリ言語で検索・調査できるようにした。

## 動機

- EBM は ID 参照ベースのグラフ構造であるため、通常のデバッグ手法が使いづらかった:
  - **デバッガ**: ID 参照はただの数値として表示されるので、参照先を追うのが困難。
  - **debug print のテキスト出力**: グラフ構造をテキストに落としても、
    grep で探すと同名のノード（例: FUNCTION_DECL）が大量にヒットする。
    知りたいのは「この STRUCT_DECL の子の FUNCTION_DECL はどこか」であって、
    全 FUNCTION_DECL ではない。
  - **grep で数値 ID を辿る**: 参照先の ID をひとつずつ grep していく体験が苦痛だった。
- クエリ言語にしたのは、上記のような「条件付きでノードを探す」操作を
  効率的に行うため。

## 具体例

```bash
# 特定の条件で Statement を検索
./tool/ebmgen -i file.ebm -q "Statement { body.kind == \"IF_STATEMENT\" }"

# -> で参照先のノードに飛んで条件を書ける
./tool/ebmgen -i file.ebm -q "Statement { body.field_decl.field_type->body.kind == \"VECTOR\" }"
# field_type は TypeRef で、-> によって参照先の Type ノードを辿っている
# EBM の内部構造（TypeRef 等）がクエリに漏れ出ているが、デバッグ専用なので許容
```

## これは X を意味しない

- 汎用クエリ言語を目指しているわけではない。EBM のデバッグに特化したツール。
