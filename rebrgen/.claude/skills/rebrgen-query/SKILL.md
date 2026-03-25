---
name: rebrgen-query
description: ebmgen の EBM インタラクティブクエリエンジンの使い方。EBM ファイルの内容を調査・デバッグしたいときに参照する。
---

## 起動方法

```bash
# インタラクティブモード
./tool/ebmgen -i <path/to/input.ebm> --interactive
# → `ebmgen>` プロンプトが表示される

# 単発クエリ（非インタラクティブ）
./tool/ebmgen -i <path/to/input.ebm> -q "Statement { body.kind == \"IF_STATEMENT\" }"

# 結果をテキスト形式で
./tool/ebmgen -i <path/to/input.ebm> -q "Expression { body.kind == \"LITERAL_STRING\" }" --query-format text

# ID 直接指定
./tool/ebmgen -i <path/to/input.ebm> -q "42" --query-format text
```

## クエリ構文

```
<ObjectType> { <conditions> }
```

インタラクティブモードでは先頭に `query`（または `q`）が必要:
```
q Statement { body.kind == "IF_STATEMENT" }
```

### ObjectType

| 指定 | 検索対象 |
|------|---------|
| `Statement` | statements テーブル |
| `Expression` | expressions テーブル |
| `Type` | types テーブル |
| `Identifier` | identifiers テーブル |
| `String` | string literals テーブル |
| `Any` | 全テーブル |

複数ブロックは OR として結合される。

### フィールドアクセス

| 記法 | 意味 |
|------|------|
| `.` | 構造体メンバーアクセス (例: `body.kind`) |
| `[]` | コンテナ要素アクセス (例: `fields[0]`) |
| `->` | Ref のデリファレンス (例: `body.target->body.kind`) |

### 演算子

- 比較: `==`, `!=`, `>`, `>=`, `<`, `<=`
- 論理: `and`/`&&`, `or`/`||`, `not`/`!`
- 包含: `contains`（例: `Any { contains 123 }`）

### リテラル

- 数値: `123`、`0xff`
- 文字列: `"IF_STATEMENT"`

## クエリ例

```
# MATCH_STATEMENT の最初の分岐が RETURN のもの
q Statement { body.kind == "MATCH_STATEMENT" and body.match_statement.branches[0]->body.match_branch.body->body.kind == "RETURN" }

# id が 0〜99 の Expression
q Expression { id >= 0 and id < 100 }

# BINARY_OP の add 演算
q Statement { body.kind == "BINARY_OP" and body.bop == "add" }
```
