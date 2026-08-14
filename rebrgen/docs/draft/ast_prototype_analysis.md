# AST プロトタイプ: 現状分析と設計軸

## この文書の位置づけ

draft。判断ではなく分析である。2026-08-05 の議論の記録で、**現 AST を残したまま別構造を
プロトタイプとして試す**ための材料を集めたもの。ADR ではないので「こうする」とは書いていない。

事実は `file:line` を付けた。確認していない箇所は §7 に分離した。

- 対象: `brgen` 側の `src/core/ast/` `src/core/middle/` `src/tool/gen/` `astlib/`
- 関連: `docs/rebuild_retrospective.md` (既存アーキテクチャの棚卸し。本文書とは別軸)

---

## 0. 出発点

「今の AST は扱いにくい。今のは今ので残しつつ、プロトタイプ的にもっといい AST 構造に
できないか」という問題意識。その根として挙がったのが:

1. AST の解析 (typing 周辺) をもう少しちゃんとやりたい。そのツケが下流に回っている
2. parser が既に typing 解析の一部をやり始めている
3. **union 周りの設計を parser でやるべきではなかった。** union を if/match から導出すること
   自体は妥当だったが、それを **struct union と if/match の 2 つに分離した**のが現構造の縛りに
   なっている (§1-9)

なお 1〜3 はいずれも**構造**の問題であり、**表現 (storage) の問題ではない**。arena 化 (軸2) は
storage の変更なので、これ単体では 1〜3 のどれも解決しない (§4 軸2 の但し書き)。

---

## 1. 現 AST の構造

### 1-1. ノード表現

継承 + runtime tag。`Node` (`src/core/ast/node/base.h:22`) が `const NodeType node_type` と
`lexer::Loc loc` を持ち、全ノードが派生する。ノード種の SSOT は
`src/core/ast/node_type_list.h` (364 行) で、`traverse.h` (213 行) がそれを回す。

### 1-2. 構文と解析結果が同じノードに同居している

```cpp
// base.h
struct Type : Node {
    bool is_explicit = false;
    bool non_dynamic_allocation = false;
    BitAlignment bit_alignment = BitAlignment::not_target;
    std::optional<size_t> bit_size = std::nullopt;  // "if dynamic length or not decidable, nullopt"
};
struct Expr : Node {
    std::shared_ptr<Type> expr_type;
    ConstantLevel constant_level = ConstantLevel::unknown;
};
// expr.h:11
struct Ident : Expr {
    std::string ident;
    scope_ptr scope;
    std::weak_ptr<Node> base;
    IdentUsage usage = IdentUsage::unknown;
};
```

パース直後と typing 後が同じ型なので、**`bit_size == nullopt` が「まだ解析していない」なのか
「動的だから決まらない」なのか型では区別できない**。解析を足すことは既存ノードにフィールドを
足すことと同義になり、`--dump-types` の出力が変わって生成ライブラリ全体に波及する (§1-4)。

`Ident` の構文的実体は `ident` と `loc` だけで、`scope` / `base` / `usage` は全部 derived。

### 1-3. `Scope` はカクタススタック、`base` はその memoize

```cpp
// src/core/ast/node/scope.h:8
struct Scope {
    std::weak_ptr<Scope> prev;                    // 親 (上向きは非所有)
    std::vector<std::weak_ptr<Ident>> objects;    // このスコープの宣言
    std::shared_ptr<Scope> branch;                // 下へ
    std::shared_ptr<Scope> next;                  // 同レベル
    std::weak_ptr<Node> owner;
    bool branch_root = false;
    lexer::Loc loc{};   // "Used by language servers to resolve which scope contains a given source position"
};
```

`lookup_current` (`scope.h:22`) は `objects` を**逆順**に走査し (後方宣言優先 = shadowing)、
`self` が現れるまでスキップし (自己参照回避)、見つからなければ `prev.lock()` で親へ上がる。
`got->branch.get() == this` なら打ち切る。

`objects` は vector で名前索引ではないため、探索は
**O(スコープ鎖の深さ × 各スコープの宣言数)**。`Ident::base` はこの探索結果のメモ化であり、
一度解決すれば二度と鎖を歩かない。

`branch` / `next` が `shared_ptr` で `prev` が `weak_ptr` なのは、所有権の循環を切るため。

### 1-4. スキーマは既にデータ化されている (ただし C++ の従属物)

```
src2json --dump-types  →  JSON のノード定義
      ↓
gen_ast2{go,ts,rust,py,c,csharp,dart,mermaid} + cpp_deep_copy + enum_gen
```

`src/tool/gen/type.go` の `Type { Name, IsArray, IsPtr, IsWeak, IsInterface, IsOptional }` が
各言語の型表現を決める (Rust の `Rc<RefCell<T>>` / `Weak<...>` の選択もここ)。

**新しい AST を試すのに 8 言語分のライブラリを手書きする必要はない**が、スキーマは
`dump_types()` (`src/tool/src2json/src2json.cpp:264`) が C++ の型を反映したものであって、
独立した定義ではない。C++ の表現を変えると生成 API が巻き添えになる。

### 1-5. JSON 境界は既にフラット配列 + index

```cpp
// src/core/ast/json.h:32
std::unordered_map<std::shared_ptr<Node>, size_t> node_index;
std::unordered_map<std::shared_ptr<Scope>, size_t> scope_index;
std::vector<std::shared_ptr<Node>> nodes;
```

```ts
// astlib/ast2ts/src/ast.ts  parseAST()
const tmpstruct_type = on.body.struct_type === null ? null : c.node[on.body.struct_type];
```

`collect(Node)` (`json.h:63`) が辿るのは `shared_ptr` と `vector<shared_ptr>` (所有辺) **のみ**で、
`weak_ptr` は辿らない。`found != end` の早期 return は所有辺が複数経路で同じノードに届く場合の
重複排除であって、循環検出ではない。

**index 採番の理由は「`weak_ptr` 参照をどうシリアライズするか」であり、それ以外の用途はない。**
`encode()` が `weak_ptr` に当たったときに採番済みの番号を出せるようにするためのもの。

したがって **ポインタグラフは C++ と TS のメモリ内表現であって、境界は既に index グラフ**。
C++ でポインタグラフ → JSON で index に平坦化 → TS/Go/Rust で再びポインタに復元、という
3 回の変換が、C++ 内でポインタ操作を書けることのために存在している。

### 1-6. 外部インターフェースは CLI が主、ライブラリはその薄いラッパ

```c
// src/tool/src2json/capi.h
typedef void (*out_callback_t)(const char* str, size_t len, size_t is_stderr, void* data);
S2J_EXPORT int libs2j_call(int argc, char** argv, CAPABILITY cap, out_callback_t out_callback, void* data);
```

1. **API が `argc` / `argv`**。呼び出し側は CLI 引数文字列を組み立てる。ebmgen は実際に
   `{"libs2j", "--no-color", "--print-json", "--print-on-error", input, ...}` を作り、
   stdin データすら `--sized-argv` / `--sized-argv-size` で argv に載せている
   (`rebrgen/src/ebmgen/main.cpp:254`)
2. **出力が単一コールバックで、`is_stderr` 引数が多重定義されている**。
   `IS_STDERR(x) = ((x) & 1) == 1` と `IS_DIRECT_AST_PASS(x) = ((x) & (1<<9)) != 0` が
   同じ引数を共有する。「stdout/stderr の別」と「ペイロードの種別」が 1 つの整数に同居
3. **状態が thread_local グローバル**。`entry.cpp:55-56` の `out_callback` /
   `out_callback_data`、`src2json.cpp:215-216` の `cout_color_mode` / `force_print_ok`。
   スレッド安全ではあるが**同一スレッドで再入不可**
4. **診断は基本テキスト**。`--print-on-error` で JSON にも載るが、構造化された診断を返す口はない。
   例外は `DirectASTPassInterface` で、`error: const SourceError*` を渡すのでこの経路だけ構造化される

`Capability` に `lexer` / `parser` / `importer` が別フラグとして存在するが、**これは段階分離の
ためではない** (作者証言、§6)。10 フラグはすべて「**呼び出し側に何をさせないか**」の制限であり、
動機は (a) 呼び出し側が変な argv を指定できないようにする、(b) DLL として使われるときに
勝手に network を立てたりしないようにする、の 2 点。段階を呼び分けるという発想は無かった。

したがって**「どこまで走らせるか」を指定する口は現状存在しない**。CLI フラグ
(`--print-json` 等) が出力形式を決めることで結果的に停止段階が決まる、という形になっている。

### 1-7. このコストを最も払っているのは LSP

```ts
// lsp/server/src/server.ts:117
const ch = spawn(exe_path, command);
```

LSP は **`src2json` をプロセスとして起動**し、`lexerCommand` (トークン) と `parserCommand` (AST)
で別々に 2 回呼ぶ (202, 210, 246 行)。編集のたびに **プロセス起動 → 全再パース → JSON 直列化 →
JSON パース → `parseAST` のポインタ復元**が走り、増分処理は一切ない。

### 1-8. `Stream` (トークンストリーム) が持つ操作

`src/core/ast/stream.h` (135 行)。parser が唯一触る字句層のインターフェース。

```cpp
std::list<lexer::Token> tokens;   iterator cur;
std::optional<iterator> last_skip;      std::optional<iterator> prev_skip_pos;
File* input;   size_t line, col;
std::vector<std::shared_ptr<Comment>> comments;   bool collect_comments;
lexer::Option lex_option;
```

| 分類 | 操作 |
| --- | --- |
| 位置移動 | `eos()` / `consume()` / `backward()` / `prev_token()` / `loc()` |
| 先読み | `peek_token()` / `peek_token(Tag)` / `peek_token(string_view)` |
| 条件消費 | `consume_token(Tag)` / `consume_token(string_view)` |
| 判定のみ | `expect_token(Tag)` / `expect_token(string_view)` |
| 必須消費 | `must_consume_token(Tag, hint)` / `must_consume_token(string_view, hint)` |
| スキップ | `skip_space()` / `skip_space_comment()` / `skip_line()` / `skip_white()` |
| 回復 | `recover_to_prev_skip()` |
| 診断 | `report_error(...)` `[[noreturn]]` / `token_error(Tag or string_view, hint)` |
| モード | `set_collect_comments(bool)` / `set_regex_mode(bool)` |
| バッファ | `shrink()` / `take()` / private `maybe_parse()` |
| コメント | `get_comments()` |

**字句解析が遅延かつ文脈依存。** `maybe_parse()` が必要時にトークンを生成し、
`set_regex_mode(bool)` / `set_collect_comments(bool)` で **parser が lexer の挙動を実行時に
切り替える**。字句解析が構文解析の状態に依存しているため、**lexer 単体の切り出しは
見た目ほど単純でない**。軸4 の「lex / parse / bind / type を別々に呼べる」はここに当たる。

**バックトラックの原語がほぼ無い。** `backward()` (1 トークン) と `recover_to_prev_skip()`
(直前の skip 位置へ) のみで、汎用の save/restore は無い。`Stream` は自身の `std::list` への
iterator を保持するため、コピーによる位置保存も安全に取れない。parse.cpp での使用は
`backward()` が 1 箇所 (542 行)、`recover_to_prev_skip()` が 4 箇所 (172, 557, 709, 1239 行)。

これは設計判断ではない。**そもそもバックトラックする想定で作られていない** (作者証言、§6)。
エラートレラント性は後から付けたもので、`recover_to_prev_skip` / `prev_skip_pos` /
`last_skip` はその後付け分にあたる。汎用バックトラックが「必要だが避けた」のではなく、
**要求として存在していなかった**。

**エラーが例外。** `report_error` は `[[noreturn]]` で `error(...).report()` を投げ、
`enter_stream` の `try/catch` が `result<T>` に変換する。**parser 内部は例外ベース、境界で
expected** という構成。プロジェクトの「No exceptions; use `futils::error::Error<>`」という
方針とこの層だけ別扱いになっている (構文解析の脱出には例外が楽なので意図的と思われる)。
軸4 で「構造化診断を返す」形にするなら、この境界の位置が決めごとになる。

**`std::list` + iterator。** ランダムアクセス不可。`shrink()` が
`tokens.erase(begin, prev_skip_pos ? *prev_skip_pos : cur)` で前方を捨てるのでストリーミング前提の
設計だが、`enter_stream` が `cur = tokens.begin()` から始めるため全保持で動く経路もありそう (未確認)。

細かい点: `skip_space()` のヘッダコメントは `// Tag::space, Tag::comment` だが、実装は
`skip_tag(lexer::Tag::space)` のみ (`stream.cpp:197`)。space+comment は `skip_space_comment()` の
ほう。コメントの誤り。

### 1-9. union の型ビューが構文の複製になっている

**現構造の縛りの本体はここ**という位置づけ (§0-3)。

#### parser が作っているもの

`parse_match` (`parse.cpp:349` 付近) は構文木と同時に型ビューを作り、双方向に結ぶ:

```cpp
std::shared_ptr<StructUnionType> union_ = std::make_shared<StructUnionType>(match->loc);
match->struct_union_type = union_;
union_->base = match;                       // 双方向リンク
```

さらに union field の合成 (`parse.cpp:288-341`) では:

```cpp
auto union_type = std::make_shared<UnionType>();
auto ident = std::make_shared<Ident>(union_type->loc, k);   // 合成 Ident
ident->usage = IdentUsage::define_field;
ident->scope->push(ident);                                   // スコープに登録
auto field = std::make_shared<Field>(union_type->loc);       // 合成 Field
...
while (c->cond.lock() != type->conds[cand_i]) {
    union_type->candidates.push_back(get_null_cache(cand_i)); // 位置合わせの null padding
    cand_i++;
}
state.add_to_struct(std::move(field));
```

つまり parser の中で以下が走っている:

1. 構文木の構築 (`Match`, branches)
2. **型ビューの導出** (`StructUnionType` / `UnionType` / `UnionCandidate`)
3. **合成ノードの生成とスコープ登録** (`Ident` + `Field` を新規作成して `scope->push` / `add_to_struct`)
4. **意味論エラー検査** (duplicate field name をブランチ横断で比較)
5. **位置合わせの null padding** (`get_null_cache`)

#### 「分離」の実体は複製である

```
Match (構文)  ←→  StructUnionType (導出)
                     conds[]      ← branch の条件を コピー
                     structs[]    ← branch の body を コピー   // size must equal to conds.size()
                     union_fields[] → 合成 Field
                                        field_type = UnionType
                                                       candidates[]  ← conds[] と添字整合
                                                                        欠けは null padding
```

導出ビューが構文を**参照せず複製している**。だから添字で整合を取る必要が生じ、
`// size must equal to structs.size()` というコメント不変条件と `get_null_cache` の
padding 機構が要る。関与するノード型は 4 つ (`StructType` / `StructUnionType` /
`UnionCandidate` / `UnionType`) で、相互に `weak_ptr` で結ばれている。

#### 下流はこれを引き継いでいる

EBM 側で観測した以下は、すべてこの AST 構造の帰結である:

- `MATCH_STATEMENT.branches[i]` ↔ `STRUCT_UNION.variant_desc.members[i]` の**位置暗黙対応**
  (`MatchBranch.body` は 7 branch とも null で、中身は型側にある)
- `StructUnionDesc.lowered_match_statement` が**分岐条件の唯一の在処**になっていること
- ADR 0041 の two views

ADR 0041 は two views を「union という 1 箇所に閉じているから成立する」と正当化しているが、
**AST 側では 4 型に散っており、それを正当化する記録は無い** (§6 のとおり基礎層に ADR が無い)。

#### 複製をやめた場合

構文木を `Match { cond, branches[{cond, body}] }` だけにし、union ビューを binder の導出結果
(side table) に置く:

```
UnionView { match: MatchId,
            fields: map<name, vector<pair<BranchIndex, FieldId>>> }   // 疎。padding 不要
```

- 構文の複製が無いので**同期ズレが構造的に起きない**。`conds.size() == structs.size()` の
  不変条件が不要になる (branches が単一の正本)
- `get_null_cache` の null padding が消える (存在しない branch は写像に無いだけ)
- 合成 `Ident` / `Field` を parser が scope に push する必要がなくなる →
  **parser がスコープを触る理由が 1 つ減る** (§3 に直接効く)
- EBM 側も導出結果をそのまま受け取ればよく、位置暗黙対応が伝播しない

**これは軸1 (解析結果を side table へ) の具体例であり、最も痛い箇所でもある。**

#### 下流を調べる理由は「役割の把握」であって「見積もり」ではない

合成 `Field` は `state.add_to_struct()` で構造体のメンバ列に入っており、下流は
**合成 Field を「普通の Field」として扱っている**と思われる。side table に移すと
「構文木上の Field 列」と「意味論上の Field 列」が別物になる。

**この事実は設計の入力であって、変更を止める理由ではない** (§5「並置が前提」)。
調べる目的は「合成 Field が下流で**何の役割を果たしていたか**」を把握して、新設計が
その役割を別の形で満たせるようにすることであり、影響範囲の大きさを理由に見送るためではない。

#### 調べた結果 (2026-08-11)

**区別している。ただし印ではなく型で。** 合成かどうかを示すフラグは存在せず、
`StructUnionType` / `UnionType` が判別子そのものになっている。

合成されるものは 2 つある:

1. match / if ごとの**無名 Field** — 型が `StructUnionType`
2. 名前ごとの**Field** — 型が `UnionType`。`union_fields` に入り、かつ
   `add_to_struct` で構造体のメンバ列にも入る

消費者は両方の経路で触っている:

- 型から: `json2c/generate.h:113` と `json2cpp2/generate.h:247, 858` が
  `StructUnionType::union_fields` を回す
- メンバ列から: `json2cpp2/generate.h:567` が全フィールドを回しながら
  `if (ast::as<ast::StructUnionType>(field->field_type)) { continue; }` で 1 を飛ばし、
  `if (ast::as<ast::UnionType>(field->field_type))` で 2 を特別扱いする

2 の特別扱いは識別子の**文字列手術**になっている (`generate.h:567-571`, `1022-1026`):

```cpp
if (ast::as<ast::UnionType>(field->field_type)) {
    if (ident.starts_with("(*") && !ident.starts_with("(*this")) {
        ident.erase(0, 2);  // remove '(*'
        ident.pop_back();   // remove ')'
    }
```

この手術は union フィールドのアクセスが getter 呼び出しに写されていることに由来する:

```cpp
// generate.h:469
str.map_ident(union_field->ident, "(*", prefix, "." + union_field->ident->ident + "())");
```

union フィールドは存在しないことがあるので、アクセスがポインタ / optional を返す
getter に写る。`(*x.name())` がその形で、素のメンバ名が要る文脈で包みを剥がしている。
**union がある限り生成器はこれを描く。** 構造をどう変えても消えない。

#### 何が何の対価かを分ける

この節の前半 (§1-9 冒頭) で挙げた問題は 3 つあり、原因が別々である。混ぜないこと。

| 症状 | 原因 | 消すには |
| --- | --- | --- |
| `conds.size() == structs.size()` の不変条件、`get_null_cache` の null padding | 導出ビューが構文を**複製**している | 複製をやめる (branches を正本にする)。置き場所は関係ない |
| parser が合成 Ident / Field を scope に push する | 導出を**parser でやっている** | 導出を後段へ移す。ビューの形は関係ない |
| 生成器の getter 化と `(*...)` の剥がし、メンバ列での場合分け | union という**機能そのもの** | 消えない |

**解析結果を side table に置くこと (軸1) はこのどれの直接の原因でもない。**
複製をやめる / 導出を後段へ移す、をやった結果として「導出ビューをどこに置くか」が問われ、
その答えの候補が side table である、という順序になる。逆向きに
「side table にすれば解決する」と読むと、3 行目まで解決するかのように見えてしまう。

---

## 2. 責任の層のズレ

「AST の解析をちゃんとやりたい、そのツケが下流に回っている」の内訳。

| 導出したい事実 | 性質 | 実装位置 |
| --- | --- | --- |
| ブロック単位の性質 (`BlockTrait` 24 flags) | 言語非依存 | middle ✓ |
| bit size / alignment / sizeof / recursive | 言語非依存 | middle ✓ (`type_attribute.cpp`) |
| decl 単位の mutation (ADR 0034) | 言語非依存 | ebmgen `ConverterState` |
| CFG (動的ビットフィールド解析) | EBM 構造依存 | ebmgen transform (位置は妥当) |
| **式の文脈中立性 / 純粋性** | 言語非依存 | **未実装** |
| **呼び出しグラフ越しの `fill_buf` 伝播** | 言語非依存 | **`ebm2rust` のみ** |

**式の文脈中立性**: `rebrgen/src/ebmgen/convert/type.cpp:95` に

```cpp
// TODO(on-keyday): currently, use Encode for condition evaluation, but ideally,
// we should use other mechanism by analyzing expressions strictly
const auto _mode = ctx.state().set_current_generate_type(GenerateType::Encode);
EBMA_CONVERT_EXPRESSION(overall_cond_ref, n->cond);
```

union の判別条件を Encode 文脈で変換している。宣言的ターゲットが欲しいのは文脈中立な述語なので、
ここを経由すると encode 文脈の lowering が乗った形を掴むことになる。

**`fill_buf` 伝播**: `fill_buf` / `HasFillBuf` の出現は `rebrgen/src/ebmcg/ebm2rust/` の 6 ファイル
のみ。ADR 0037 の判断 (`&mut impl Read` か `BufRead` か) は Rust 固有で正しいが、その入力である
「どの decode 関数が推移的に until-EOF 終端判定を要するか」は**言語非依存の事実**であり、それが
1 バックエンドの `DefUseCollector` の中にある。他 17 言語は同じ事実を持たない。

`BlockTrait` は 24 フラグ (`procedural` / `conditional` / `control_flow_change` / `read_state` /
`dynamic_order` / `static_peek` / `magic_value` / `bit_stream` 等) あり、
「middle が解析していない」わけではない。落ちているのは**式単位の性質と手続き間の伝播**である。

---

## 3. parser と middle の境界が引かれていない (両方向)

当初この節は「parser が意味論に踏み込んでいる」という**一方向**で書いていたが、不正確だった。
ズレは両方向にある。

| 向き | 例 |
| --- | --- |
| **意味論側に踏み込んでいる** | スコープ構築 / 束縛 / `usage` 設定 / union 型ビューの導出 (§1-9) |
| **構文で決まることを後段に回している** | `Assert` (文位置の真偽式) / `IOOperation` (メンバ名の文字列照合) |

### 後段に回っている側

**`Assert`** — `replace_assert.h:81` は「文リストの要素が `Binary` で演算子が boolean op」で判定する。
**純粋に構文的**で、名前解決も型も要らない。parser が文リストを作る時点で判定できる。
後段にある理由は見当たらない。

**`IOOperation`** — こちらは**意図がある** (作者証言、§6)。

```cpp
// parse.cpp:656 — base は構文で特別扱いしている
if (auto i = s.consume_token("input"))  { return make_shared<SpecialLiteral>(i->loc, SpecialLiteralKind::input_); }
if (auto o = s.consume_token("output")) { return make_shared<SpecialLiteral>(o->loc, SpecialLiteralKind::output_); }
if (auto c = s.consume_token("config")) { return make_shared<SpecialLiteral>(c->loc, SpecialLiteralKind::config_); }
```

識別子より先に `consume_token` するので `input` / `output` / `config` は普通の `Ident` にならず
`SpecialLiteral` になる。**実質的に予約済み。**

一方でメンバ名は文法に焼かず、`Ident` のまま残す:

```
input.get(u8)
   ↓ parser
Call( MemberAccess( SpecialLiteral(input_), Ident("get") ), args )
       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^^^^^^^^
       構文で特別扱い済み                   ただの Ident
   ↓ middle (resolve_io_operation)
extract_config.h:32  return conf + "." + d->member->ident;   // ドット連結で文字列化
                     "input.get" と照合 → IOOperation に置換
```

**これは中途半端ではなく設計。** base だけを special にしておけば、`input.` / `output.` /
`config.` の後ろに**後から好き勝手にメンバを継ぎ足せる**。parser を触らずに機能を増やせる、
という拡張性のための線引きだった。実際 `config.go.package` / `config.url` /
`config.go.union` / `config.endian.little` / `config.bit_order.lsb` のように任意のドット列が
使われている。

### 残る観察: open namespace と closed set に同じ機構を使っている

| | 性質 | メンバ集合 |
| --- | --- | --- |
| `config.*` | 生成器へ渡す属性。**開いた集合** | ユーザー/生成器が自由に足す |
| `input.*` / `output.*` | 言語のプリミティブ。**閉じた集合** | `get` / `peek` / `backward` / `subrange` / `put` を `resolve_io_operation` が列挙 |

拡張性の設計は `config.*` には素直に効くが、`input.*` / `output.*` は
コンパイラが意味を知っている閉じた集合であり、性質が異なる。同じ `SpecialLiteral` 機構を
共有しているため同じ扱いになっている。

なお同じ名前照合は複数層で行われている (`resolve_io_operation.h:70-85`、
`typing.cpp:1832` の `conf->name == "input"`)。parser が構造として持っていた情報を
文字列に戻して照合する形になっている。

### (旧) parser が意味論に踏み込んでいる側

`src/core/ast/parse.cpp` (1,818 行) は `ScopeStack stack` (26 行目) を持ち、パース中に
スコープを構築しながら以下を設定する:

- `ident->usage` — 15 種の値
- `ident->scope` — 所属スコープ
- `ident->base` — 宣言への束縛 (321, 764, 892, 1341, 1415 行など)
- `ident->expr_type` — 型 (324, 1340, 1409 行)
- `enum_->enum_type->bit_size = enum_->base_type->bit_size` (1458 行) — 型属性の計算
- `lookup_current` の呼び出し (854 行)

### `usage` の書き手は 3 層に分かれる

| 書き手 | 意味 | 値 |
| --- | --- | --- |
| `parse.cpp` | **構文上の役割** | `define_*` 11種 / `reference_member` / `bad_ident` |
| `typing.cpp` | **解決結果** | `reference` / `reference_type` / `reference_member_type` |
| `replace_*` / `resolve_*` / `monomorphize` / `extract_config` | **書き換えマーク** | `reference_builtin_fn` / `reference_type` |

`maybe_type` は parser → typing の受け渡し用の暫定値。

`reference_builtin_fn` は 5 つの pass (`typing.cpp` / `resolve_available.h` /
`resolve_io_operation.h` ×2 / `replace_error.h` / `replace_metadata.h` /
`ast/tool/extract_config.h`) が同じスロットに書く。これは ident の分類ではなく
**「その pass が置換を行った」記録**であり、上書きなので誰が付けたか追えない。

### 同一 enum である実利

```cpp
// src/core/middle/typing.cpp:1360
if (base->usage == ast::IdentUsage::define_format || ... ||
    base->usage == ast::IdentUsage::define_state ||
    base->usage == ast::IdentUsage::define_type_parameter) {
    ident->usage = ast::IdentUsage::reference_type;
}
else {
    ident->usage = ast::IdentUsage::reference;
    if (base->usage == ast::IdentUsage::define_field) { register_state_variable(ident); }
}
```

typing は **「定義側の構文役割」を読んで「参照側の解決結果」を決めている**。同じ enum だから
この dispatch が一行で書ける。構文役割と解決結果を別テーブルに分離すると、ここは 2 テーブル
操作になり冗長化する。**分離しやすいのは「書き換えマーク」を `usage` から外すほう**で、
構文役割と解決結果の分離はこの対価がある。

---

## 4. 設計軸

### 軸1: 解析結果を side table に出す

AST は構文だけを持ち、解析結果は `NodeId → Info` の別表に置く。

- **得る**: middle が解析を足しても AST 構造が変わらない → `--dump-types` の出力が不変 →
  `gen_ast2*` 再生成と LSP/web の連動が起きない。§2 の「解析を上げると rebrgen だけで
  閉じない」対価が消える。`Ident` の `scope`/`base`/`usage` は全部 derived なので、
  side table 化は妥協ではなく**正しい分類**になる
- **失う**: `node->expr_type` が `table.type_of(node)` になる。LSP のようにノードから型を引く
  用途が主な消費者は記述量が増える。`expr_type` は現在 JSON AST スキーマの一部なので、
  外に出すなら互換方針が要る

代案の `Ast<Parsed>` / `Ast<Typed>` 型パラメータ化は、C++ で全ノード型がテンプレートになり
生成 8 言語側にフェーズ概念を持ち込むため、side table より波及が大きい。

**最も具体的な適用先は union ビュー (§1-9)。** 抽象的に「解析結果を分離する」ではなく
「union の型ビューを構文から分離する」なら、消えるもの (構文の複製 / 位置整合の不変条件 /
null padding / parser のスコープ操作) と対価 (合成 Field の扱い) が具体的に言える。
軸1 を試すなら、ここを最初の対象にするのが情報量が高い。

### 軸2: ポインタグラフ → ID + arena

> **但し書き: 軸2 は storage の変更であって構造の変更ではない。**
> arena 化で得られるのは `collect()` 消滅・`deep_copy` 単純化・`kill_node` 消滅の 3 つで、
> これは配管の改善である。ノード集合は 74 種のまま、フィールドもそのまま、`Ident` が宣言と参照を
> 兼ねるのもそのまま、union の 4 型複製 (§1-9) もそのまま。**§0 に挙げた 1〜3 のどれも
> 軸2 単体では解決しない。** 軸2 を「AST を良くする」施策として数えないこと。

- **得る**: `json.h` の `collect()` / `node_index` / `scope_index` が不要になる (§1-5 のとおり
  weak_ptr のシリアライズのためだけに存在するため)。`deep_copy.h` 6,206 行の `IsWeak` 特別扱いも
  消え、arena のコピーに落ちる。JSON の出力データは変わらない (既に index)。
  `base` の deref が `arena[id]` のベクタ添字になり、`weak_ptr::lock()` のアトミック操作が消えるので
  **ホットパスはむしろ速くなる**
- **失う**: `->` チェーンが `arena[...]` になる記述面。`--dump-types` が C++ の型を反映するため、
  `IsPtr`/`IsWeak` が落ちて生成 8 言語の API が変わる。LSP が使う「復元済みポインタ」を維持するには
  スキーマ側に「実体は index だが対象言語では参照として提示する」注釈が要る (= 軸3)

**当初の見積もりは逆だった**: 「EBM で `access_helper.hpp` 10,691 行が必要になったのと同じコストを
AST でも払う」と見ていたが、境界が既に index なのでこれは誤り。EBM が `access_helper` を必要と
したのは全ノードが ID だからではなく `get_field<"path">` で kind 別 union を深く辿るためであり、
AST の arena 化がそれを要求するとは限らない。**コスト中心は `typing.cpp` (2,076 行) の書き換え量**
であって、シリアライズ境界でも LSP でも性能でもない。

`Scope` については、arena なら所有が一本化されて weak/shared の区別が消えるため、
`branch`/`next` を持たず**親 ID + 宣言リストだけで足りる可能性がある** (未確認、§7)。

### 軸3: スキーマを C++ から独立させる (その先に `.bgn` 定義)

EBM は `.bgn` 定義 (ADR 0004) なのに、AST は C++ 手書き + `--dump-types` で抽出という逆向きの構成。
スキーマを独立させれば内部表現と提示形式を別々に決められ、軸2 の波及を止められる。
その先で AST も `.bgn` で定義すれば dogfooding が一貫するが、`.bgn` は shared_ptr グラフや
`scope_ptr` を表現できないため**軸2 が前提**になる。

### 軸4: parser を最初から外部利用しやすいライブラリにする

現状 (§1-6) は CLI が主で、`libs2j_call` はその薄いラッパ。プロトタイプで作り直すなら、
最初からライブラリとして設計する。

- **入力**: ソーステキスト + import 解決コールバック。**parser がファイルシステムを知らない**
  (現在の `S2J_CAPABILITY_FILE` / `IMPORTER` による禁止ではなく、依存そのものを持たない形)
- **出力**: 戻り値で AST + 構造化診断のリスト。コールバックのタグ多重定義をやめる
- **状態**: 明示的なコンテキストオブジェクト。グローバルなし、再入可能
- **段階**: lex / parse / bind / type を別々に呼べる。現在の capability は「禁止」の粒度であって
  「段階を呼び分ける」粒度ではない
- **`argv` を経由しない**。設定は構造体

**§3 と同じ場所に効く。** parser が「構文 AST + 診断」だけを返す関数になれば、
`usage` / `base` / `expr_type` を書く余地が構造的に無くなる。binder を別の呼び出しにすれば
side table (軸1) の置き場所も自然に決まる。**API 分離と phase 分離は同じ作業になる。**

**得る**: LSP が §1-7 のコスト (プロセス起動 + 全再パース + JSON 往復 + ポインタ復元) を
払わなくなる余地ができる。段階を呼び分けられれば、トークンだけ欲しい要求で typing を走らせずに済む。

**失う / 対価**:

- ADR 0023 が ecosystem を deprioritize と決めている。「外部から使いやすく」は外部利用者を
  想定した投資に見えるため、方針と表面上ぶつかる。ただし**現時点の最大の外部利用者は LSP と
  ebmgen という自前のツール**なので、ecosystem 向けというより自分のための整備という読み方はできる
- CLI を捨てるわけではない。`src2json` の main は残り、薄いラッパになる方向
- **capability の 10 フラグは全部「呼び出し側への制限」であって段階指定ではない** (§1-6, §6)。
  この機能自体は残す価値がある (DLL 利用者が勝手に network / file を使わないようにする)。
  段階分離は別の軸として新たに足すことになり、既存フラグを流用する話ではない

#### `argc`/`argv` 形式を変えても元の目的は失われない

この形式の動機は「**`src/tool/brgen` (Go) からプロセス起動しまくるのが遅いのをどうにかしたい**」
であり (作者証言、§6)、DLL 化してプロセス起動コストを消すことが目的だった。
argv 形式そのものは**既存 CLI をそのまま DLL 越しに呼べる**という実装コストの安さから来ており、
API 設計としての意図ではない。

したがって構造体ベースの API に変えても**元の目的 (プロセス起動の回避) は失われない**。
バイナリデータを argv に載せるための `--sized-argv` / `--sized-argv-size` (作者いわく苦肉の策) も、
構造体 API なら不要になる。ここは「意図を壊す変更」ではない。

### 軸5: 言語機能表を最初から定義する

現状、「brgen にどんな機能があり、どのバックエンドが対応しているか」は**全部 derived か暗黙**である。

| 何 | どこ | 形 |
| --- | --- | --- |
| 文法 | `web/doc/content/docs/bnf.md` + `parse.cpp` | 人間向け散文 + 実装 |
| 機能の列挙 | `example/feature_test/` 34 ファイル | **ファイル名が機能名** |
| 分類 | `BlockTrait` 24 / `IdentUsage` 19 / `NodeType` / EBM の 3 種の Kind | 散在、粒度も不一致 |
| バックエンド対応 | 存在しない | `ebmcg/*/visitor/` のファイル名集合から導出 |
| 未対応の一覧 | ADR 0048 の「変換できない 58 件」 | **実測で得たもの** |

`example/feature_test/` に `trial_match.bgn` / `state_variable.bgn` / `type_parameter.bgn` /
`sizeof.bgn` / `for_in.bgn` / `enum_is_defined.bgn` などが並んでいるのが実質の機能表だが、
**ディレクトリであって表ではない**。機能 X が「どの構文 / どの AST ノード / どの EBM ノード /
どのバックエンド」に対応するかを繋ぐものがない。

そのため ADR 0048 では「変換できない 58 件」を得るのに全 `.bgn` を走査して最深フレームで分類する
測定が必要になっている。宣言があれば差分で出るはずのもの。

#### 腐らせないための分業

全部を宣言にすると「表に載っているが実装がない」「実装したが表にない」が必ず発生する。
特に破壊的変更を続ける MVP フェーズ (ADR 0026 / velocity over adoption) では、表のメンテが
ボトルネックになる。**分業を先に決めるのが要:**

- **宣言するのは「機能の存在と識別子」だけ** — feature ID / 名前 / 説明 / 紐づく `feature_test/*.bgn`
- **対応状況は測定から埋める** — バックエンド対応欄は hook の有無や unictest 結果から生成する。
  人が書かない

こうすると表は「ID の台帳」になり、対応状況の列は自動で埋まる。`survey_bgn_constructs.py` が
既に行っている測定を feature ID に紐づけて出す形になり、ADR 0048 の 58 件も
「宣言された feature のうち変換が通らないもの」として出る。

#### 「最初から」が効く理由

作り直しの文脈で効くのは、**feature ID を parser / AST / EBM / backend が共通して参照できる**点。
AST ノード種と EBM ノード種がそれぞれ「どの feature に属するか」を宣言していれば:

- 新バックエンド追加時に実装すべき一覧が出る。現在 `ebmtemplate.py list <lang>` で分かるのは
  **実装済み hook** であって、未実装の全体像は出ない
- unictest の入力が「どの feature を覆っているか」を言える。現在は EBM ノード種の有無で測っており、
  ADR 0048 自身が「ノード種別の**有無**しか見ていない」と限界を明記している
- `language.md` を表から生成できる

#### 粒度をどこで切るか

`BlockTrait` の粒度 (`terminal_pattern` / `dynamic_order` / `full_input`)、EBM ノード種の粒度
(`SUB_BYTE_RANGE` / `INIT_CHECK`)、構文の粒度 (`trial_match` / `state`) は一致しない。
どの層で切るかを決めないと表が 3 種類できて余計に散る。

`example/feature_test/` のファイル名が示す実際の直感は**構文・意味論の層**
(`trial_match` / `state_variable` / `type_parameter` / `for_in` / `enum_is_defined`)。
ここを正本にして AST ノード種と EBM ノード種を「その feature の実装手段」として下に紐づけるのが、
既存の運用と整合しそうに見える (未検証)。

#### 測定: AST ノード種では機能を区別できない (2026-08-09)

上の「未検証」を測った。`feature_test/*.bgn` を nast のパーサに通し、木に現れるノード種の集合を
ファイルごとに取って重なりを見た (31/34 が解析可能。残り 3 は後述)。

| 見たもの | 結果 |
| --- | --- |
| 他のどのファイルにも無いノード種を持ち込むファイル | **7 / 31** |
| ノード種集合が他ファイルに完全に含まれるファイル | **17 / 31** |
| ノード種の総数 | 59 種 |

新規ノード種を持ち込むのは `comma_match` (CharLiteral/OrCond)、`regexp` (NamedArgument/
RegexLiteralType)、`sort_test` (Metadata)、`tree_test` (Break/Continue)、`type_parameter`
(GenericType)、`union` (StrLiteralType)、`union_member_access` (Available) の 7 つ。
**`trial_match` / `state_variable` / `sizeof` / `exhaustive_check` / `nested_state` /
`state_variable2` はいずれも新規ノード種を持ち込まず、うち 4 つは他ファイルの完全な部分集合である。**
`Binary` / `If` / `Match` / `Field` の組み合わせでできているためで、機能の区別はその層に存在しない。

初回 (2026-08-09) はこの表を「5 / 31」「6 / 31」「31 種」と記録していたが、これは誤りだった。
当時 `nast` の pretty printer が `if` を含む入力でスタックオーバーフローしており
(`BodyStatement.belong` に `weak` が付いておらず `If -> blocks[] -> belong -> If` で循環していた)、
落ちたファイルが空出力のまま「解析成功・ノード種ゼロ」として数えられていた。
ノード種ゼロの集合は他のどの集合の部分集合でもあるため、部分集合の数が過小に、
総ノード種数も過小に出ていた。**結論の向きは変わらない**が、数値は上の通り。

ADR 0048 が EBM 側で自ら書いている限界 (「ノード種別の**有無**しか見ていない」) と同じ形が、
一層上の AST でも成立している。**したがって feature ID はノード種から導出できず、宣言するしかない。**
一方で対価は無く、対応状況の列は測定で埋められる — 上の分業案はこの測定と矛盾しない。

#### 実体

`brgen/spec/features.json` に置いた (schema: `brgen/spec/brgen_features_schema.json`)。
62 feature。ID は `F0034-trial-match` のように ADR と同じ「番号 + slug」形式。
`covers` で `feature_test/*.bgn` と多対多に紐づく。宣言するのは存在と識別子だけで、
バックエンド対応欄は持たない。

`python script/check_features.py` が機械的な整合だけを見る — `covers` のパスが実在するか、
`feature_test/*.bgn` がどれかの feature から参照されているか、ID と slug が一意か。
「実装されているか」は見ない。

#### 落とし穴: 既定で働かない機能がある

`F0062-error-tolerant-parsing` は `ParseOption.error_tolerant` を立てたときだけ働き、
既定は fail fast である。この区別を落とすと、`error_tolerant.bgn` のような
**意図的に構文誤りを含む入力**が落ちるのを実装の欠落と読み違える。実際に読み違えた。
対応状況を測るときは、機能ごとに「どのモードで測るのが正しいか」が要る。
`covers` だけでは足りない情報がここにある。

### 順序

軸1 は単独で価値がある (§2 の対価が消える)。軸2 は JSON 境界を壊さないがスキーマ経由で生成 API に
波及するため、波及を制御するなら軸3 の前半 (スキーマ独立化) を先に置くほうが扱いやすい。

軸1 と軸2 は独立ではなく `base`/`scope`/`usage` で交わる。typing にとっては**埋めていく作業領域**、
LSP にとっては**解析後の照会面**であり、時相の異なるものを同じフィールドが兼ねている。

軸4 は §3 (parser から意味論を剥がす) と同じ作業なので、この 2 つは分けて数える意味が薄い。
「parser を段階分離されたライブラリにする」ことが、そのまま「parser が `usage`/`base` を
書かなくなる」ことになる。

---

## 5. 前提: 並置戦略、下流コストは判断基準にしない

**現行実装は残したまま新構造を並置する。** したがって:

- **「下流の変更が大きいから見送る」は理由として採らない。** 影響範囲の大きさは移行の話であって
  設計の話ではない。並置期間中は現行パイプライン (json2* / LSP / web / ebmgen) が動き続ける
- 下流を調べるのは**設計の入力**としてであり、見積もりのためではない。「この構造は下流で何の役割を
  果たしていたか」が分かれば、新設計がその役割を別の形で満たせる。役割を知らずに作ると
  知らずに機能を落とす (§6)
- 本文書の各軸に書いた「失うもの」は**トレードオフの記述**であって却下理由ではない。
  ダメなら実際に作ってダメだと分かる、という順序で進める

この前提は §1-9 (union ビューの分離、影響範囲が最大の部類) に特に効く。**影響の大きさは
着手しない理由にならない。**

## 5-2. プロトタイプの入口

**JSON AST を入力にするのがいちばん安い。** パーサに触らず、`src2json` の出力を新構造に変換する層
として始める (ebmgen が `load_json` を持つのと同じ構図)。

- 既存パイプライン (json2*, LSP, web) は無傷
- 新 AST で型解析を書いてみて、実際に楽になるかを測れる
- ダメなら捨てられる

§2 で挙げた解析 (式の純粋性、手続き間伝播) をどのみち書くなら、それを新 AST 上で書くのが
プロトタイプの中身として自然。

parser から意味論を剥がす (§3) のは、この入口の次の段階になる。パース中のスコープ構築は
ブロック構造がその場で分かるぶん安く、分離すると binder の 1 パスが増えるが、それ自体のコストは
無視できる。実コストは `parse.cpp` から `state.current_scope()` を剥がす書き換え量。

---

## 6. 基礎層には設計記録が無い

本文書を書く過程で繰り返し当たったのは、**`src/core/` の設計意図がどこにも記録されていない**
という点である。

| | ADR |
| --- | --- |
| `rebrgen/docs/decisions/` | **48 本** (EBM / ebmgen / ebmcodegen / ebmcg / テスト運用) |
| brgen 本体 (`src/core/`, `src/tool/src2json/`) | **ディレクトリ自体が存在しない** |

リポジトリ全体で `decisions/` ディレクトリは `rebrgen/docs/decisions` の 1 つだけ。
48 本のうち `src/core` に言及するのは ADR 0048 が 1 本のみで、それも
「エンディアン定数を `src/core/ast/tool/eval.h` と一致させる」という参照であって、
`src/core` 側の設計判断ではない。ADR 0018 (bgn-syntax-choices) は `.bgn` の**言語仕様**の判断で、
実装構造の判断ではない。

### これが何を意味するか

上層 (BM → EBM) は**一度作り直しを経験しており**、その過程の判断が ADR として残っている。
基礎層 (lexer / parser / AST / middle) は最初に書かれたまま作り直しを経ておらず、かつ
当時の判断が記録されていない。結果として:

- **意図的な設計と、たまたまそうなったものが区別できない。** 実装を読んでも「なぜこの形か」は
  分からない。本文書の §7 (未確認事項) が長いのはこのため
- 本文書の作成過程で、`src/core` について実装の形から推測した断定が 4 回外れた
  (JSON 往復の有無 / MATCH の条件保持 / `ConverterState` の保護 / `usage` の書き手)。
  一方 rebrgen 側は ADR を読めば裏が取れた。**記録の有無がそのまま推測の当たり外れに出ている**

ADR 0026 は「切る判断を ADR 化せず実装だけ進めると、今後誰か (user 自身含む) が
『なぜこんな設計にした』と訝しがった時に意図が復元できなくなる恐れ」を却下理由として書いている。
**その状態が基礎層で起きている。** ADR を書き始めたのが rebrgen 以降だったという時系列の結果であり、
基礎層の設計が悪かったという話ではない。

### 「意図の復元」ではなく「依存の把握」

当初この節は「現行の判断が意図的かどうかを先に確定させる必要がある」と書いていたが、
**その前提が誤っている**。基礎層の形の相当部分は判断ですらない。

作者本人の証言として、`Ident::base` / `scope` / `usage` をノードに置いたのは
**当時 side table という概念を知らなかったから**であり、設計判断ではない。**無知であって意図ではない。**

これは基礎層について一般に当てはまる可能性が高い。当時の知識の範囲で書けた形がそのまま残っている、
というだけのものに「なぜこの設計か」を問うても答えは出ない。

したがってプロトタイプで問うべきは:

| 誤った問い | 正しい問い |
| --- | --- |
| これは意図的な設計か | **今これに何が依存しているか** |

**原設計に意図が無くても、下流が依存していれば制約は制約である。** `base` / `scope` / `usage` は
無知の産物だが、typing と LSP の両方が現に使っている。「意図が無いから自由に変えてよい」には
ならない。制約が**意図的なものから経験的なものに変わる**だけで、確認は要る。

逆に言えば、確認すべき対象が「作者の意図」(復元不能かもしれない) から「現在の依存関係」
(grep と実行で確定できる) に変わるので、**作業としてはむしろ扱いやすい**。§7 の未確認事項は
その意味での作業リストである。

### 「当時知らなかった」は記録できる

ADR 0003 には既に前例がある:

> ID 参照ベースなのは深い設計思想というより、extended_binary_module.bgn で定義する上での都合、
> ポインタ直挿しへの置き換え手法を当時知らなかったこと、json2cpp2 の生成の限界など
> 実際的な制約の積み重ねによるもの。

**「当時の知識が及ばなかった」は ADR に書ける内容**であり、実際 EBM については書かれている。
基礎層に欠けているのはこの種の記録である。意図が無かったこと自体を記録すれば、
後から読む側が「深い理由があるのでは」と探す時間が消える。

### 3 分類 — 「意図あり」「当時の規模では妥当」「無知」

当初この節は「意図あり / 意図なし」の二分だったが、**3 つ目のカテゴリが要る**。

| 分類 | 内容 | 対処 |
| --- | --- | --- |
| **意図あり** | 目的があって選んだ | その理由が今も成立するかを検査する。失効していれば変えてよい |
| **当時の規模では妥当** | 判断は正しかったが、後から前提 (規模・消費者数・パイプライン長) が変わった | **今の規模で実際に困っているか**を確認してから動く。困っていなければ放置が正解 |
| **無知** | その概念・手法を知らなかった | 知識を足せば直る。困っているかどうかとは独立に、直す価値は基本的にある |

真ん中が独立したカテゴリである理由は、**再設計の必要性そのものが規模依存**だからである。
「無知」は知識を足せば常に改善だが、「規模が変わった」は今の規模を確認しないと
改善かどうかが決まらない。

**実際には単一分類にならない項目が多い。** 作者いわく「半分無知、半分『とりあえず速く動くものに
する』点でそのほうが都合がよかった」。基礎層の最初期は **SecHack365 の成果物として動くものを
出す必要があり、時間的な圧があった**という事情がある。

この「時間制約下の近道」は独立した要因として扱う価値がある:

- 無知とは違う (知っていても選んだ可能性がある)
- 規模とも違う (規模ではなく締切が制約だった)
- **制約が既に消えている**点が重要。当時の理由は「今も成立するか」を検査するまでもなく失効している

したがって扱いは「意図あり」より「無知」に近く、**困っているなら直してよい**。ただし
近道であっても結果的に良い設計になっている場合はあるので、近道であること自体は欠陥の証拠に
ならない。

### 判明分 (2026-08-05 時点)

証言 = 作者本人の説明。推論 = 本文書がコードと経緯から立てた読み。

| 対象 | 分類 | 根拠 | 内容 |
| --- | --- | --- | --- |
| `Ident::base` / `scope` / `usage` のノード配置 | **無知** | 証言 | side table という概念を知らなかった。ただし typing と LSP が現に依存 |
| parser がスコープ構築と束縛を行う | **無知** | 証言 | 段階分離という精緻な概念を持って作っていない |
| `Stream` に汎用バックトラックが無い | **当時の規模では妥当** | 証言 + 推論 | そもそもバックトラックする想定で作っていない。エラートレラント性は後付けで `recover_to_prev_skip` 系がその分。**要求が無かっただけ**で、今も困っていないなら放置でよい |
| union を parser 内で導出し、構文と型ビューに分離した (§1-9) | **当時の規模では妥当** | 推論 | 導出自体は妥当 (証言)。パイプラインが「1 つの生成器で終わり」なら parser 内導出は普通の設計。縛りになったのは後から IR が挟まり 18 バックエンド + LSP + web が同じ AST を消費するようになったため。**判断が誤りだったのではなく前提が変わった** |
| `input`/`output`/`config` を `SpecialLiteral` にし、メンバ名は文法に焼かない (§3) | **意図あり** | 証言 | base だけ special にしておけば、後から `input.` / `output.` / `config.` の後ろに好き勝手にメンバを継ぎ足せる。parser 無改修で機能を増やすための拡張性設計。実際 `config.go.package` / `config.url` 等で使われている |
| `libs2j_call` の `argc`/`argv` 形式 | **意図あり** | 証言 | `src/tool/brgen` (Go) からのプロセス起動が遅いので DLL 化したかった。argv 形式は既存 CLI の流用で、`--sized-argv` は苦肉の策 |
| capability | **意図あり (別目的)** | 証言 | 呼び出し側が変な argv を指定できないようにする / DLL 時に勝手に network を立てないようにする。**段階分離の意図は無い** |

### この分類の使い方

「当時の規模では妥当」の 2 件は、**今の規模で困っているかどうかで扱いが分かれる**:

- **union (§1-9) は困っている。** 位置暗黙対応が EBM まで伝播し (`branches[i]` ↔ `members[i]`)、
  `lowered_match_statement` が分岐条件の唯一の在処になっている。前提が変わった影響が実際に
  下流で観測できるので、着手対象になる
- **`Stream` のバックトラックは困っていない。** parse.cpp の使用は `backward()` 1 箇所と
  `recover_to_prev_skip()` 4 箇所のみ。要求が今も無いなら**放置が正解のまま**

### 推測が外れる理由

「意図あり」は 2 件だけで、しかも**本文書が推測していた意図とは別だった**
(`argc`/`argv` は WASM の都合ではなくプロセス起動コスト、capability は段階分離ではなく
呼び出し制限)。残り 4 件は意図が存在しないか、存在しても「当時の規模での妥当性」であって
今のコードから読み取れる形では残っていない。

**「合理的な設計判断があったはず」という前提そのものが誤り**であり、存在しない意図は推測できない。
本文書が `src/core` について 4 回誤ったのはこれが理由である。

痕跡は残っている場合がある。`kill_node.h` の冒頭コメント:

```cpp
// for large scale of AST
// destructor call stack is too deep that makes stack overflow
// so we need to use this
```

これは**実際にスタックを溢れさせてから書かれたもの**で、当初の設計が想定していた規模を
超えたことの記録になっている。`--sized-argv` (苦肉の策)、`Stream` へのエラートレラント性の
後付けも同型。**こうした「後から足したもの」は、前提が変わった地点の化石として読める。**

### 意図は保護ではなく評価の入力

ここまでの書き方は「意図があれば守る、無ければ変えてよい」と読めるが、**それは誤り**。
設計思想があることは、それを維持する理由にならない。**ダメな判断ならダメなだけである。**

正しくは対称で、意図の有無は「変えてよいか」を決めず、**検査の手順を 1 段変えるだけ**:

| | やること |
| --- | --- |
| 意図なし | 依存関係を確認する。それだけ |
| 意図あり | 依存関係を確認する + **その理由が今も成立するか**を検査する。失効していれば変えてよい |

意図の記録が価値を持つのは、この 2 段目の検査を可能にする点にある。理由が分かれば
「その前提はまだ真か」を問えるが、理由が分からないと問えないので変更が博打になる。
**記録は判断材料であって拘束ではない。**

この扱いはプロジェクトの既存方針と一致している。ADR 0026 は自ら
「**『切った軸は永遠に切ったまま』ではない**。遠い将来、先行事例の積み重ねから成立する可能性は
排除しない。現段階で切っているだけ。scope 切りの判断自体を ADR として残す理由は、将来の再統合の際に
『なぜ一度切ったか』が失われないようにするため」と書いている。ADR は将来の再検討のための入力として
書かれており、決定を凍結するためのものではない。

本文書で確定した意図 2 件も、この観点で見れば変更を妨げない:

- `argc`/`argv` — 目的はプロセス起動コストの回避。**構造体 API でも同じ目的を達成する**ので、
  理由が変更を妨げていない (軸4)
- capability の呼び出し制限 — 目的は今も有効なので**機能としては残す**。ただし「段階分離に流用する」
  という当初の本文書の読みが誤りだっただけで、制限機能自体が変更を妨げているわけではない

### 言語は戻り値ベース / モジュール化 / グローバル副作用なしで成立するように作る (作者証言 2026-08-10)

**これは言語の話であって、処理系の話ではない。**

`.bgn` の意味論は、戻り値ベースで、モジュール化されていて、グローバルな副作用が無い、
という制約の下で表現できるように設計する。個々の実装がその制約を破ること自体は構わない。
**言語が少なくともその制約下で成立していればよい**、という下限の置き方である。

言語側で効いている形として、`state` (F0014) が隠れたグローバルではなく宣言された型であること、
`resolve_state_dependency` が format ごとに `state_variables` を明示的に伝播させることがある。
フォーマット間で状態を渡す手段が、言語の語彙として明示されている。

§1-6 に挙げた `libs2j` の thread_local グローバル (`out_callback` / `cout_color_mode`) は
**処理系の実装**の話であり、この節の制約とは別の軸である。両者を同じ基準で評価しないこと。

## 7. 未確認事項

断定していないもの。プロトタイプに進むなら先に確認すべき順。

1. **`typing.cpp` が `base` / `scope` をどう走査・更新しているか。** 軸2 の実コストはここに集中する
   はずだが、2,076 行を読んでいない
2. **`lookup_backward` (`scope.h:53`) が `next` / `branch` を使っているか。** 使っていれば
   「親 ID + 宣言リストだけ」案は成立しない
3. **`branch` / `next` が lookup 以外に使われているか。** `collect(Scope)` による列挙用途は確認済み
   (arena なら列挙は iterate で済む) だが、他は見ていない
4. **`bad_ident` を binder に遅らせたときの診断品質。** parser が早期にエラーを出せることが
   エラー位置や復帰の質を支えている可能性がある
5. **ADR 0034 の mutation 解析が ebmgen にあるのが妥当か。** §2 の表では「妥当」としたが、
   ADR の意図を読んだ上ではなく実装位置からの推測
6. **`libs2j_call` が `argc`/`argv` 形式である理由。** WASM (emscripten) 経由の呼び出しや
   web playground の worker が引数文字列を前提にしている可能性がある。`web/dev/src/lib/` 側の
   呼び出し形式を確認していない
7. **capability の権限制御が誰の要求か。** `network` / `file` / `std_input` の禁止は
   web playground のサンドボックス要求由来と思われるが確認していない。段階分離と分けるなら
   ここの要求元が要る
8. **`union_fields` / `member_candidates` / `is_strict_common_type` が下流で果たしている役割。**
   §1-9 に進むなら最初に読むべき箇所。**コスト見積もりのためではなく**、新設計が同じ役割を
   満たせるようにするため (§5)。特に `state.add_to_struct()` で入った合成 `Field` を
   typing / ebmgen / LSP が構文上の Field と区別しているか、していないか。していないなら
   それが「区別する必要が無かった」のか「区別できなかった」のかまで見る

---

## 関連

- ADR 0002 (generics pre-monomorphize) — `deep_copy.h` 6,206 行の存在理由。AST の形を変えても
  コピーの必要性自体は消えない
- ADR 0003 (AST→IR→Code) — EBM の ID 参照は「深い設計思想ではなく実際的制約の積み重ね」と
  明記されている。**EBM がそうだから AST もそうすべき、という論拠にはならない**
- ADR 0004 (EBM を `.bgn` で定義) — 軸3 の先にあるもの
- ADR 0034 (mutation analysis) / ADR 0037 (BufRead propagation) — §2 の表の根拠
- `docs/rebuild_retrospective.md` — 既存アーキテクチャの棚卸し。本文書とは別軸
