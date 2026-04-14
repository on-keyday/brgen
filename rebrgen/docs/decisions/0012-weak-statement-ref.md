# WeakStatementRef と StatementRef の使い分け

## 日付

- 判断時期: 2025年12月26日（DAG構造のためにWeakStatementRefを導入）
- 文書化: 2026-04-15

## 判断

EBM の参照を StatementRef（strong）と WeakStatementRef（weak）に分け、
traverse_children の自動生成時に DAG として扱えるようにした。

## 動機

- traverse_children 関数を自動生成する際、すべての参照を同等に辿ると
  親への参照で無限再帰が発生する。strong/weak のマーカーがないと
  DAG として扱えない。

## 具体例

使い分けの基準は C++ の shared_ptr/weak_ptr と概念的に同じ:

- **StatementRef (strong)**: 木構造で所有する子を指す場合。
  例: FunctionDecl.body, Block 内の子 Statement。
- **WeakStatementRef (weak)**: 親を指したり、所有関係にない参照の場合。
  例: FunctionDecl.parent_format（親の StructDecl を指す）、
  EnumMemberDecl.enum_decl（親の EnumDecl を指す）。

## これは X を意味しない

- メモリ管理のための区分ではない。EBM はフラットな物理レイアウトで ID 参照ベースなので
  循環参照によるメモリリークは起きない。あくまで traverse 時の走査制御のため。
