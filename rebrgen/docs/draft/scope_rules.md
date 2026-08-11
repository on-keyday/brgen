# 現行のスコープ解決規則

`src/core/ast/node/scope.h` (195 行) と、それを使う `parse.cpp` / `middle/typing.cpp` を読み、
`example/feature_test/lookup_phase.bgn` を `src2json` に通して裏を取ったもの。調査日 2026-08-11。

nast 側で束縛を書き直すときの参照用。**現状の記述であって、こうすべきという話ではない。**

## 1. 入れ物

```cpp
// scope.h:8
struct Scope {
    std::weak_ptr<Scope> prev;                  // 親、または同じ段の 1 つ前の区間
    std::vector<std::weak_ptr<Ident>> objects;  // この区間の宣言
    std::shared_ptr<Scope> branch;              // 内側へ
    std::shared_ptr<Scope> next;                // 同じ段の続き
    std::weak_ptr<Node> owner;
    bool branch_root = false;
    lexer::Loc loc{};                           // LSP が位置からスコープを引くための範囲
};
```

**1 つの段が複数の区間に割れる。** `ScopeStack::maybe_init` (scope.h:162) が

```cpp
if (current->branch && !current->next) {
    current->next = std::make_shared<Scope>();
    current->next->prev = current;
    current->next->owner = current->owner;
    current = current->next;
}
```

としているので、内側のブロックを 1 つ作った後に同じ段で宣言を続けると、**新しい区間 (`next`) に入る**。
つまり「ブロックの前の宣言」と「ブロックの後の宣言」が別オブジェクトに分かれ、`next` で繋がる。
`prev` は親でもあり前の区間でもある、という二役を持つ。

`enter_branch` (scope.h:177) が `branch` を作り、そこに `branch_root = true` を立てる。
根も `branch_root = true` (scope.h:166)。

## 2. 引く口が 3 つある

### `lookup_backward` (scope.h:53) — 通常の名前解決

`typing.cpp:876` の `ident->scope->lookup_backward(search, ident)` がこれ。

1. **自分の区間を逆順に**見る (後の宣言が勝つ = shadowing)
2. `self` が現れるまで飛ばす。**ただし `is_type_ident(obj)` なら飛ばさない** (scope.h:58)
3. `only_type_allowed` が立っていれば型の識別子だけを見る
4. `prev` へ上がる。このとき
   - 自分が `branch_root` で、`owner` が **format / state / enum** なら
     **`only_type_allowed = true`** にして上がる (scope.h:75-83)
   - `may_forward` は `may_forward || this->branch_root`
5. それでも無ければ `next` へ `lookup_forward(fn, true)` — **型限定**で前方を見る

### `lookup_forward` (scope.h:98)

`objects` を宣言順に見て、`next` を辿る。`fn` には常に `may_forward = true` を渡す。

### `lookup_current` (scope.h:22)

自分の区間を逆順、`self` まで飛ばし、`prev` へ。ただし
**`got->branch.get() == this` なら打ち切る** — 内側から外へ出ない。
`parse.cpp:854` の `check_duplicated_def` 専用で、名前解決には使われない。

### 型の識別子とは

```cpp
// scope.h:46
bool is_type_ident(const std::shared_ptr<Ident>& ident) {
    return ident && (ident->usage == IdentUsage::define_format ||
                     ident->usage == IdentUsage::define_enum ||
                     ident->usage == IdentUsage::define_state ||
                     ident->usage == IdentUsage::define_type_parameter);
}
```

`define_field` / `define_variable` / `define_const` / `define_fn` は**含まれない**。

## 3. 出てくる規則

| | 前方参照 | 外側のスコープから見えるか |
| --- | --- | --- |
| format / enum / state / 型パラメータ | **できる** | **見える** |
| field / 変数 / 定数 / fn | できない | format / state / enum の境界を越えると**見えない** |

`lookup_phase.bgn` を `src2json --print-json` に通した実測 (`usage` / `base` は解決結果):

```
line  3  data       usage=unknown          base=None   fn の中から、まだ宣言していない field
line  6  Data       usage=reference_type   base=30     9 行目で宣言される format を先に参照
line  8  data       usage=reference        base=15     宣言済みの field
line 11  data       usage=unknown          base=None   入れ子の format から外の field
line 13  Data       usage=unknown          base=None   global から format Lookup の中の Data
line 14  Lookup     usage=reference_type   base=3      global の format
```

6 行が全部、ファイル内のコメントが書いている期待どおりになる。

**11 行目が `only_type_allowed` の効き目**。`format Data` は `format Lookup` の中にあるが、
`branch_root` かつ `owner` が format なので、外側へ上がるときに型だけに絞られ、
外の `data` (field) は見えない。

**6 行目が「型は前方参照できる」**。同じ段の `next` を `lookup_forward(fn, true)` で見に行く。
型限定なので、同じ位置に field があっても引っかからない。

## 4. global への fallback

```cpp
// typing.cpp:876
auto found = ident->scope->lookup_backward(search, ident);
if (found) { return found; }
global_search = true;
return current_global->lookup_forward(search);
```

`lookup_backward` で見つからなければ、**global を `only_type_allowed = false` で前方に舐める**。
上の表の「外側から見えない」は `lookup_backward` の話で、global だけは最後に全部見に行く。
13 行目の `Data` が解決しないのは、`Data` が global の `objects` に居ないから (format Lookup の中に居る)
であって、fallback が型限定だからではない。

### 効いている非対称

`lookup_backward` の同じ段の前方探索は型限定 (`next->lookup_forward(fn, true)`) だが、
この fallback は型限定でない。結果として **型でない名前を前方参照できるのは global にあるときだけ**
になる。`recurse_defs.bgn` を通した実測:

```
x ::= y          # 1 行目
format X:
    x ::= y      # 4 行目。ファイルのコメントは "global y"
    y ::= u16()  # 5 行目
    z ::= y      # 6 行目。"local y"
y ::= z          # 8 行目
```

```
line 1 col  7  y  usage=reference  base=27   8 行目の global の y。前方参照が通っている
line 4 col 11  y  usage=reference  base=27   同じく global の y
line 6 col 11  y  usage=reference  base=12   5 行目の local の y
```

4 行目が global に落ちるのは、その時点で local の `y` がまだ宣言されていないため。
ファイル自身が `# global y` / `# local y` と書いている。

**この非対称は意図されている。** 根拠は 2 つ。

1. このファイルの global の定義は `x ::= y` / `y ::= z` / `z ::= y` で**循環している**。
   1 行目の前方参照が解決しなければ循環が構成されず、`recurse_defs` というテストが成立しない
2. `typing.cpp:1344-1353` に、解決先の定義にまだ型が付いていない場合
   (`def && !def->expr_type`) その定義を先に型付けしに行く機構がある。
   `recurse_detect` はその再入を止めるためのもので、**前方参照が解決するからこそ要る**。
   後方参照だけなら解決先は既に型付け済みである

```cpp
recurse_detect.insert(ident.get());
auto guard = futils::helper::defer([&] { recurse_detect.erase(ident.get()); });
typing_expr(bin->right);
typing_assign(bin);
```

## 5. スコープを作る構文

| 呼び出し | 何 |
| --- | --- |
| `parse.cpp:207` `new_indent` | インデントブロック全般 (format / fn / state の本体など) |
| `parse.cpp:1486` `new_indent` | enum の本体 |
| `parse.cpp:359` `cond_scope` | match |
| `parse.cpp:384` `cond_scope` | match の分岐 (ScopedStatement) |
| `parse.cpp:453` `cond_scope` | if |
| `parse.cpp:1100` `cond_scope` | for |

`if` / `match` / `for` は**条件部にもスコープを持つ** (`if_->cond_scope` など)。
`for x in ...` の `x` がループ全体に見えるのはこのため。本体のブロックはさらに内側になる。

## 6. 気づいた点

- **`prev` が二役**。親スコープでもあり、同じ段の前の区間でもある。`branch_root` が
  どちらかを判別する唯一の手がかりで、`lookup_backward` の `only_type_allowed` も
  `lookup_current` の打ち切りも、この 1 ビットに乗っている
- **解決は 2 段構え**。`lookup_backward` に外れたら global を舐める。前者だけを読むと
  「外側は型しか見えない」と読めるが、global は例外
- **`may_forward` が使われていない**。`lookup_backward` / `lookup_forward` は `fn` に
  第 2 引数として渡すが、`typing.cpp:860` の `search` は受け取って捨てている。
  前方参照かどうかで診断を変える余地として置かれたまま使われていない
- `objects` は名前索引ではなく vector なので、探索は
  **O(区間の鎖の長さ × 各区間の宣言数)**。`Ident::base` がその memo

## 7. 未確認

- 型パラメータ (`define_type_parameter`) が `is_type_ident` に入っているが、
  スコープのどの段に push されるか

§4 の非対称と `recurse_detect` は確認済みなのでここから外した。
