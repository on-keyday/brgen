# unictest コーパスは構造粒度で飽和、当面は ebmgen の変換対応を進める

## 日付

2026-07-29

## 判断

unictest の入力コーパスは EBM ノード種別の粒度では既に飽和しており、`example/` から実在
フォーマットを追加しても新しい構造はほぼ増えない。したがって当面は、テストデータを増やす方向
ではなく **ebmgen が変換できない `.bgn` を変換できるようにする方向**を進める。
判明した 3 種の欠落（`LITERAL_INT64` / `LITERAL_CHAR` / `Statement CONTINUE`）だけは安いので
先に埋める。

## 動機

- **enough to represent formats**。変換できない 72 個の `.bgn` は、表現力の穴がそのまま
  リストになったもの。ここを潰すことが表現力目標に直結する。
- 測定結果が「フォーマットを増やす」案を否定したため。推測ではなく数字で方向が決まった。

### 測定結果 (2026-07-29, `tool/ebmgen` で全 `.bgn` を走査)

| | |
| --- | --- |
| 現コーパス | 21 フォーマット / **95 種の構造** |
| コーパス外で変換可能な `.bgn` | 245 個 |
| そのうち新しい構造を 1 つでも足すもの | **19 個** |
| 足される構造の総数 | **3 種** |
| 変換できない `.bgn` | **72 個** |

足される 3 種の内訳:

| 構造 | 保有する候補数 | 代表 |
| --- | --- | --- |
| `Expression LITERAL_INT64` | 15 | `asn1`, `ebml`, `protobuf`, `pe_header`, `gpt`, `minecraft` |
| `Expression LITERAL_CHAR` | 3 | `tar`, `pg_wire`, `fb_dnsrocks` |
| `Statement CONTINUE` | 1 | `softether` |

変換できない `.bgn` の内訳（当面の作業リスト）。着手時点は 72 個で、下記「経過」節の分だけ
減っている。

**メッセージ単位で分類してはいけない。** `Unexpected nullptr` は `common.hpp` の
`unexpected_nullptr()` 一箇所から出る汎用エラーで、メッセージだけで括ると無関係な原因が
混ざる（16 件に見えるが実際は 5 箇所に散っている）。エラーは `handle_error` が
`std::source_location` を付けて伝播チェーンを出力するので、**最深フレームで分類する**。
その粒度では着手時点の 72 件が **38 グループ**だった。

主要なもの（件数はフレーム単位、2026-07-30 時点 = 58 件 / 35 グループ）:

| 件数 | 発生箇所 | メッセージ |
| --- | --- | --- |
| 4 | `convert/expression.cpp:746` | `Unhandled IOMethod: input_backward` |
| 4 | `convert/union_property.cpp:222` | `This is a bug: inconsistent merged size`（メッセージ自体が bug 宣言） |
| 4 | `convert/union_property.cpp:240` | `cannot get common type: UINT vs RANGE` 3 / `ENUM vs RANGE` 1 |
| 4 | `convert/expression.cpp:239` | `Expression has no type` 2 / src2json warning 2 |
| 3 | `convert/encode.cpp:147` | `EnumType without base type cannot be used in encoding` |
| 3 | `convert/encode.cpp:191` | `Unexpected nullptr` |
| 3 | `transform/bit_fields.cpp:72` | `Unexpected nullptr` |
| 3 | `converter.hpp:446` | `No current yield statement` |
| 3 | `convert/statement.cpp:418` | `Trial match is not supported yet` |
| 3 | `convert/type.cpp:149` | `IdentType has no base type` |
| 2 | `convert/encode.cpp:242` | `Array length is not specified` |
| 2 | `convert/statement.cpp:1095` | `Currently field argument must be 1` |
| 2 | `convert/decode.cpp:387` | `Invalid follow type` |
| 2 | `convert/type.cpp:244` | `Unsupported type for conversion: regex_literal_type` / src2json warning |
| 4 | (フレームなし) | src2json 側の parse error / warning |

残りは 1 件ずつのグループ。完全な一覧は
`python script/survey_bgn_constructs.py --failures -j 4`（並列度は必ず絞る）。

## 経過: 最大グループの解消（2026-07-30）

着手時点の最大グループだった `convert/expression.cpp:989` の `Unexpected nullptr` 10 件を解消し、
**72 → 65** になった。原因はここに記録しておく。

引き金は**型の循環参照**だった（`format A: c :C` / `format C: b :B` / `format B: a :A`）。
`convert_statement` は body を格納する前に visited マップへ id を登録する。これは循環で
無限再帰しないために必要な設計だが、その結果「id は有効だが body は未格納」という状態が
変換中に存在する。`encode.cpp` / `decode.cpp` の入れ子フォーマット呼び出しは
`resolve_callee_params()` で callee id から FUNCTION_DECL を引いて ADR 0034 のパラメータ
紐づけをしていたため、循環時にこの未格納状態に当たっていた。

修正は、`resolve_callee_params()` を使うのをやめ、`ConverterState::format_encode_decode`
（`add_format_encode_decode` が **body 変換より前に** PARAMETER_DECL の ref を記録している）
から直接取るようにしただけ。後段パスも順序制約も EBM 構造変更も不要だった。副次的に
「state 変数 i は params[i+1]」という位置依存も消えた。`resolve_callee_params()` は
callee 式しか手がかりが無い一般の `ast::Call` 用として残っている。

不変条件は 4 箇所にコメントとして残した: `convert_statement`（発生源）、
`add_format_encode_decode`（前倒し記録の理由）、`encode.cpp`（利用側）、
`resolve_callee_params`（誤用防止）。

結果:

- 変換できるようになった 7 件: `dhcpv6`, `loop_ref`, `java_class`, `redis_resp`,
  `recursive`, `recursive2`, `complex_case`
- より後段の別エラーへ移動した 3 件: `sort_test` →
  `Unsupported type for max value: RECURSIVE_STRUCT`、`propagate` →
  `lower_runtime_state: RuntimeState companion`、`test_cases` →
  `Unhandled IOMethod: input_backward`（既存グループへ合流）
- 回帰なし: `unictest --target-runner ebm2rust --target-option-set std-io` が
  73 PASS / 5 FAIL。失敗は既知 3 件（`bgp_open_test`, `bgp_update_test`,
  `http2_frame_inline_test`）と、新規追加入力が露出させた既知 2 件
  （`tar_single_file`, `softether_pack_unistr`）のみ。ADR 0034 の borrow/own は
  ebm2rust が最も影響を受けるため、ここが無回帰なら他も同様と判断した。

## 経過: 動的エンディアンの配線（2026-07-30）

`input.endian = <式>` が変換時に一切効いておらず、**65 → 61**。欠けていたのは 2 箇所だけで、
機能自体は EBM・converter・backend の 3 層に既に実装されていた。

- `convert/expression.cpp`: `config.endian.*` / `config.bit_order.*` を式として変換できな
  かった（`Unhandled IOMethod`）。定数形は `typing_specify_order` が `SpecifyOrder::order_value`
  へ畳み込むのでこのパスを通らないが、`endian == Endian.LittleEndian ? config.endian.little :
  config.endian.big` のような動的形は枝を式として変換する必要がある。値は
  `src/core/ast/tool/eval.h` と一致させる（big/msb=0、little/lsb=1、native=2）。畳み込み済みの
  `order_value` も `Expression_IS_LITTLE_ENDIAN` の比較（1 == little）も同じ定数を前提にしている。
- `ConverterState::set_endian`: `current_dynamic_endian` への唯一の代入が `on_function` の分岐内に
  あり、`set_on_function()` は誰も呼ばないためその分岐は実行されない。結果 `dynamic_ref` が常に
  空になり、`add_endian_specific` が `endian_expr` 無しの `IS_LITTLE_ENDIAN` を作り、動的指定が
  すべて native として生成されていた。代入を実行されるパスへ移した。

**`on_function` / `local_endian` は未使用ではなく未完成だった。** 出所は前世代の
`src/bm/convert.hpp` で、そこには `enter_function()` が `on_function` を立てて `local_endian` を
`global_endian` から引き継ぐ実装と、制御フロー合流用の phi スタックがあった。ebmgen 移行で
フィールドだけ移り進入フックが移らなかった。一度「死にコード」と判断して削除しかけたが、
`current_dynamic_endian` への唯一の代入を含むため誤りだった。

結果:

- 変換できるようになった 4 件: `elf.bgn`, `bpf.bgn`, `media/tiff.bgn`,
  `feature_test/analyze_block_trait.bgn`
- 生成コードが動的判定になった: ebm2go で
  `tmp64 := func() uint8 { if e.Endian == Endian_LittleEndian { return 1 } else { return 0 } }()`
  と各読み取りの `if tmp64 == 1`。修正前は `NativeEndian` が出ていた。
- 対象はコーパスの動的指定 16 箇所（`elf.bgn` ×3、`media/tiff.bgn` ×3、`bpf.bgn` ×8、
  `omg_cdr.bgn` ×1、`src/test/test_cases.bgn` ×1）
- 回帰なし: ebm2go std-io が 74 PASS / 4 FAIL で、失敗する入力もエンディアンスコープ修正時点と同一


## 経過: ビット連結に参加する struct メンバーの endian（2026-07-30）

`Unsupported endian type: unspec` 5 件を解消し、**61 → 58**。

`encode_field_type` / `decode_field_type` は `io_desc` を `ebm::IOAttribute{}`（endian =
unspec）で初期化し、型別処理で埋める作りだった。埋めるのは int / float / enum の 3 経路だけで、
struct / array / 文字列リテラルは unspec のまま残る。IOData がバイト列を素通しするだけなら
それで無害だが、`transform/bit_fields.cpp` はビットフィールドを連結するとき連結対象の
IOData の属性をそのまま `add_endian_specific()` に渡すため、struct メンバーが連結に参加すると
unspec が渡って落ちる。

最小再現は 7 行:

```
format Inner:
    a : ub7
    b : ub8

format Outer:
    flag : ub1
    inner : Inner      # ビット連結に参加する struct メンバー
```

対照として `flag : ub1` + `rest : ub15`（ビットのみ）と `flag : u8` + `inner : Inner`
（バイト境界なので連結されない）はどちらも通る。

**判断: 初期値を ambient endian にする。** `get_io_attribute(Endian::unspec, false)` は元々
「unspec を渡すと現在のエンディアンへ解決する」関数なので、用途どおりの使い方。int / float /
enum による上書きは従来のまま。連結時に外側フォーマットのエンディアンを採ることになるが、
各フィールドは `ub` / `ul` 接尾辞で自分のエンディアンを明示できるため、曖昧さは残らない。

`bit_fields.cpp` 側でフォールバックする案は「unspec の IOData が存在しうる」前提を残すので
採らなかった。

結果:

- 変換できるようになった 3 件: `src/test/partial_bit_union.bgn`, `3gpp_mib.bgn`, `net/tlp.bgn`
- より後段へ移動した 2 件: `media/ac3.bgn`, `ripple.bgn` →
  `transform/bit_fields.cpp:72` の `Unexpected nullptr`（同グループが 1 → 3 件に）
- 回帰なし: ebm2go std-io が 74 PASS / 4 FAIL で、失敗する入力も前段階と同一

## 具体例

測定の手順は、各 `.bgn` を `tool/ebmgen` で EBM に変換し、`ebmgen -i <ebm> -d` のテキスト
ダンプから `Statement|Expression|Type <KIND>` トークンの集合を取り、コーパス 21 フォーマットの
和集合との差分を見るというもの。

この測定に至る前は「実在フォーマットのワイヤデータを増やす」方向で TCP / HTTP/2 / DNS /
WebSocket / IPv6 に 21 サンプルを追加していた。それ自体は分岐網羅と境界値の面で意味があるが、
**構造の新規性という観点では頭打ち**であることが測定で判明した。

## これは X を意味しない

- **コーパスのカバレッジが十分という意味ではない。** 測定はノード種別の**有無**しか見ておらず、
  構造の組み合わせ・ネストの深さ・再帰の段数・長さ式の演算内容は捉えていない。同じ種別集合でも
  生成コードは大きく異なり得る。「飽和」はこの指標に関しての言明。
- **既存フォーマットへのサンプル追加が無価値という意味ではない。** 境界値（ゼロ長・等号ちょうど・
  型幅上限）は型依存でバックエンド間の差が出る領域であり、種別粒度の測定には現れない。
  ADR 0042 系の failure_case と合わせて引き続き有効。
- **72 個すべてを直すという計画ではない。** フレーム単位のグループが作業単位であり、どこから
  着手するかは別途決める。
- **`Unexpected nullptr` が 16 件の単一原因という意味ではない。** 5 つの異なる箇所から出ており、
  メッセージ単位の集計は誤り。フレームで分けること。

## 代替案

- **言語間の値レベル差分**（全バックエンドがデコード結果を正規化 JSON で吐いて突き合わせる）。
  却下。全バックエンドは同じ EBM から派生するため、IR レベルで解釈がズレていれば全言語が同じ
  間違いをして一致する。捕まえられるのは単一バックエンドの lowering ズレだけで、18 言語分の
  実装コストに見合わない。
- **外部参照実装（scapy 等）との突き合わせ。** 却下。検出できるのは `.bgn` の定義が
  プロトコル仕様と合っているかであって、brgen の変換の正しさではない。フォーマット仕様著者の
  責任側の話であり、CI に scapy 依存を持ち込む対価に見合わない（ADR 0021 の責任分界と整合）。
- **`--fuzz` を CI に載せる。** 構造の妥当性には効かない。`ebm2rmw` はモデルから入力を生成する
  ので、fuzz 入力は構成上モデルと整合する。クラッシュ耐性の検査としては別途有効。
