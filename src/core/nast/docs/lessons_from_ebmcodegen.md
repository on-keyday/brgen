# ebmcodegen から nast に活かせる設計

draft。2026-08-30 の調査記録。rebrgen の `ebmcodegen` (EBM 用のメタ生成器)
が何をやっているかを読み、nast 側の lowering を作るときに効くものと、
効かない (あるいは既にやっている) ものを分けた。

nast は既に「スキーマ 1 本から生成する」構造 (`nodes.json` → `nodegen.py`)
を持っている。これは偶然の一致ではなく、**同じ著者が EBM で得た判断を
引き継いで書いたから**である。だからこの文書の主な用途は「他所の設計に
学ぶ」ではなく、**既に下した判断のうち参照されないまま埋もれているものを
拾い直すインデックス**にある (ADR は 49 本あるが、今日の作業で実際に
効いたのは 6 本だった)。

差が残っているのは lowering 層 (フック機構と共有 util) と、出力の作り方
(CodeWriter) の 2 つ。

## 前提: nast も多言語バックエンドを持つ

`README.md` の目標がそのまま「write once generate any language code」で、
nast もそれを前提に作る。**バックエンドが 1 つで済む期間は無い**ものとして
設計する。したがって以下は「将来効くかもしれない」ではなく最初から要る:

- 言語差を受ける場所を、フックではなく **knob (宣言的な値) から**始める (B)
- 言語非依存の組み立て部品を **共有 util 層**に置く (D)。1 バックエンドの
  ときに各所へ散らすと、2 つ目で必ず引き剥がしになる
- 生成物は **共有 body + 薄いラッパ** (E)。ADR 0047 は 18 言語 150 万行の
  フルコピーを畳んだ後始末で、最初からその形にしておけば発生しない
- 「1-2 言語なら固有、3 言語以上で共通へ昇格」(ADR 0016) の運用も最初から

フック機構 (`__has_include`) だけは、置き場所の規約 (ADR 0047 の
「共有 body の隣に visitor/ を置いてはならない」) まで含めて先に決めておく
必要がある。後から入れると include 解決の基準ディレクトリを動かすことになる。

## ebmcodegen が何をしているか

`.bgn` で定義した IR を入力に、コード生成器の骨格 C++ を生成するメタ生成器。
段は 8 つあるが、要点は 3 つ。

### 1. 手書きメタデータがゼロ

`extended_binary_module.bgn` が唯一の正。`json2cpp2 --add-visit` が
`visit_static` (全フィールドを型付きで列挙する constexpr メンバ) 付きの
C++ を吐き、`structs.cpp` がそれを走査してメタ情報を組む。**構造の一覧を
手で書いた表はどこにもない**。未知の型が増えると `static_assert` で気づく。

nast も同じ構造を持っている (`nodes.json` → `gen/*.py`)。違いは、
ebmcodegen が**自分がコンパイルされるためのソース**
(`body_subset.cpp` / `access_helper.hpp`) を自分で出力し、
`update_ebm.py` が「差分があれば再ビルドしてもう一周」で固定点に収束させる
ブートストラップになっている点。

### 2. 検査を全部コンパイル時に寄せる

- パス文字列 `"body.field_decl.field_type"` は `consteval` が型グラフを
  実際に歩いて検証する。失敗時は型名・フィールド名を引数に持つラムダを
  throw して、エラーメッセージに出す
- フックファイル名とフック名の一致は
  `static_assert(std::string_view(__FILE__).contains(#name "_class.hpp"))`
- フックの解決は `#if __has_include` の連鎖。**プリプロセッサ自体が
  ディスパッチ機構**で、実行時コストゼロ
- DSL の構文自体が `static_assert` でセルフテストされる

nast の `access.h` も consteval + static_assert でパス検証している
(`field<"body.elements.0">`)。同じ考え方が引き継がれている箇所。

### 3. カスタマイズが 3 段階で、下ほど重い

| 段 | やること | 実例 |
| --- | --- | --- |
| 1 | knob (値) を代入する | `config.begin_block = ":"` (python) |
| 2 | knob (関数) を差し込む | `config.default_value_custom = [](auto& ctx){...}` |
| 3 | フックファイルを置いて乗っ取る | `visitor/Statement_BLOCK_class.hpp` |

実測 (2026-08-30): デフォルトフック 103 本に対し、言語側のフックは
**3〜30 本**。ebm2wuffs は 3 本、ebm2java は 6 本で 1 言語が成立している。
一方 `entry_before_class.hpp` での knob 設定は 50〜83 件。つまり
**言語差の大半はフックではなく宣言的な knob で吸収されている**。

`Visitor.hpp` (318 行) が knob の唯一の定義場所で、値 knob
(`begin_block` / `use_elif` / `param_type_separator`)、関数 knob
(`*_custom` / `*_wrapper`)、生成中の可変状態 (`imports` / `declared_variants`)
の 3 種が同居する。既定値は C++ のメンバ初期化子そのもの。

フック本体を書くべきなのは、knob にできない言語固有の規則。
`ebm2java/visitor/Statement_BLOCK_class.hpp` は javac の到達可能性解析
(JLS 14.21) のミラーで、これは knob 化しても他言語に意味がない
(ADR 0043 が明文化している)。

### フォールスルーの仕掛け

`pass` は特別なエラーコード (`0xba55ba55`) を持つ番兵で、`expected` の
失敗経路に相乗りする。`CALL_OR_PASS` マクロが「pass 以外なら return、
pass なら素通り」を書く。before フックが値を返すと本体を実行せず乗っ取り
(hijack)、after フックが値を返すと本体の結果を差し替える。
before/after からは `ctx.main_logic()` で本体を明示的に呼べるので、
「本体の結果を後加工する」も書ける。

**注意**: main フックは「置換」であって「チェーン」ではない。言語側が
フックを置くとデフォルト実装は完全に隠れる。共通処理に戻りたいときは
knob (`*_custom` + `CALL_OR_PASS`) を使う。

## nast に活かせるもの

### A. CodeWriter を導入する (今すぐ効く)

`unparse.cpp` は `out += ...` を 151 箇所と手動インデントで書いている。
ebmcodegen が使う `futils::code::LocWriter` は:

- 行を構造として持ち、インデントは `indent_scope()` のスコープで管理
- Writer 同士を `merge` できる (部分を組み立てて後で連結)
- **各出力片がどの IR ノード由来かを `LocEntry{loc, start, end}` で記録**
  できる (`writeln_with_loc`)

3 つ目が本命で、これは source map そのもの。
[[exit_and_reversibility]] の「どの出力がどの原文から来たか」を、
出力側からも辿れるようにする道具になる。ebmgen 側では 1 箇所しか使われて
おらず (ほぼ未活用)、nast で最初から使うほうが筋が良い。

### B. knob を「フックより先に」置く

lowering に進むとき、言語差を最初からフックで受けたくなるが、実測が示す
のは逆で、**大半は宣言的な値で足りる**。設計順序としては

1. まず値 knob (区切り文字、キーワード、真偽値リテラル…)
2. 足りないところだけ関数 knob
3. どうしても言語固有の規則が要るところだけフック

とし、「1-2 言語にしかない処理は言語側、3 言語以上に共通なら共通側へ昇格」
(ADR 0016) の運用ルールをそのまま借りる。

### C. 未実装を「空」でなく「目印」で残す

ebmcodegen は未実装フックを `{{Unimplemented KIND id}}` として出力に
埋め込み、`--debug-unimplemented` で可視化する。穴が黙って消えるより
出力に残るほうが、育てている最中の生成器には合う。
nast の unparse も現在 `/*unprintable X*/` を出しており発想は同じ。
lowering でも同じ方針を保つ。

### D. 共有 util 層を早めに切る

`stub/util.hpp` (1384 行) に `emit_struct_methods` / `handle_fields` /
`get_type_tree` / `sorted_struct` (構造体宣言順のトポロジカルソート) など、
言語非依存の組み立て部品が集まっている。粗い API を残しつつ細粒度版を
足す (ADR 0029) 運用も含めて参考になる。

nast の bind/ には共有 util 層が無く、各 cpp が自前で持っている
(`format_of_type` が requires.cpp に、`as_struct` が typer.cpp に、など)。
lowering を足す前に、複数段から使うものを 1 箇所へ寄せる。

### E. 生成物は「共有 body + 薄いラッパ」

ADR 0047: 言語別に 83.5k 行をフルコピーしていた (18 言語で約 150 万行) のを、
共有 body 1 部 + 各言語 7 行のラッパに置き換えた。名前空間はマクロで
パラメータ化する。nast は多言語前提なので、lowering の生成物を作る時点で
この形から始める (後から畳むのが ADR 0047 の作業だった)。

### F. 設計判断を ADR に残す

rebrgen は 49 本の ADR を持ち、「なぜそうしたか」「これは X を意味しない」
まで書いてある。今日の調査で効いたのは特に:

- **ADR 0003**: BM の最大の教訓 —「普通のコンパイラのように意味論を削って
  いくと後から復元困難。brgen はトランスパイラで出力先も高級言語なので、
  構造や意味を保ったまま渡す必要がある」。`exit_and_reversibility.md` は
  この判断を (参照せずに) 再導出したものだった。**既存 ADR に答えがある
  問いを議論し直していた**わけで、この文書が拾い直しのインデックスだと
  言っているのはこういう意味
- **ADR 0004**: IR 定義を `.bgn` で書く (dogfooding + bootstrap 回避)。
  nast_wire が既に踏襲している
- **ADR 0009**: 直 #include はボイラープレート 0 だが IDE が死ぬ →
  マクロ 1 つ分のボイラープレートで IDE を取り戻した
- **ADR 0007**: テストは IR レベルの単体でなく e2e (unictest) 主軸。
  「IR が正しくても生成コードが動かなければ意味がない」

nast 側は `docs/` に議論のメモが 8 本あるだけで、ADR 形式の判断記録は無い。
問題は記録量ではなく**引けるかどうか**で、rebrgen 側は判断が ADR 形式で
番号付き・「これは X を意味しない」付きで並んでいるから、今日のように
「この論点は既に決着していないか」と引きに行ける。nast の draft 群は
議論の記録であって判断の索引ではないので、同じ引き方ができない。

lowering に入ると判断が増える。少なくとも決着したもの
(as_is の encode 意味論、判別子の 2 分類、出口 v0) は ADR 形式に移す価値が
ある。移す先は rebrgen の `docs/decisions/` と分けるか同じにするかは未決。

## 活かせない / 既にやっているもの

- **Context クラスの自動生成**: EBM の kind 別 union を「その kind で有効な
  フィールドだけ持つ構造体」に落とす仕掛け。nast のノードは最初から
  種別ごとに別の型なので、この問題自体が無い
- **body_subset の総当たりプローブ**: 同上。tagged union を C++ に落とす
  ときの後始末であって、nast には該当しない
- **DSL (`{% %}` / `{{ }}`)**: 実質未使用 (サンプル 2 本のみ、しかも探索
  ディレクトリ外)。テンプレート言語より `CODE()` 相当のヘルパのほうが
  実際に使われている、という実績のほうが参考になる

## 次にやるなら

1. ~~`unparse.cpp` を LocWriter ベースに移す (A)~~ **完了** (2026-08-30)。
   インデント管理が構造化され、source map (UnparseResult::spans) も入った
2. bind/ の共有部品を util 層へ切り出す (D)
3. lowering に着手する時点で knob 表を先に作る (B)
