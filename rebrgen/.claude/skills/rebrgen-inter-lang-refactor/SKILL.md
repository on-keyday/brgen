---
name: rebrgen-inter-lang-refactor
description: rebrgen の複数言語間にまたがるリファクタリングの方法と注意点。コードジェネレーターの構造見直し、共通部分の整理、EBM構造の変化への対応などを扱う。
---

## 方法

1. ebmcg/内のコードジェネレーターの構造を見直し、共通部分と言語固有部分を明確に分ける。

このrebrgenフレームワークでは言語ごとのコードジェネレーターのうち
ebmcg/、ebmip/内では言語固有の処理を、ebmcodegen/stub/やebmcodegen/default\_[codegen|interpret]\_visitor/visitor/内では共通の処理を実装する構造になっている。
実装を進めていくと共通化可能なロジックが言語ごとに散らばることがある。これを整理して、
共通部分はebmcodegen/stub/内のファイルやebmcodegen/default\_[codegen|interpret]\_visitor/visitor/内の処理にまとめることで再利用を容易にするリファクタリングを行うことでメンテナンス性を高めることができる。

また個別hook分割されているやつについても共通化により十分短くなったならばentry_before_class.hppにまとめることも検討する。特に量が少ないうちはまとめる方が見通しが良くなる可能性が高い。

この方法はコードジェネレーターの構造を大きく変えることなく共通部分を整理することができるため比較的安全に行うことができる一方で、共通部分と言語固有部分の境界があいまいな場合は整理が難しい可能性があるため注意が必要である。

2. EBMへの変換部分をいじる

ebmgen/ディレクトリ内のコードでebmgen/transform/などに処理を追加することで
各言語固有部分をいじらずにIRレベルで全言語に影響する変更を加えることができる。
この方法は言語ごとのコードジェネレーターの構造をいじらずに済むが他方で
既存のIRの論理構造に依存しているコードが多い場合は影響範囲が広くなりすぎる可能性があるため注意が必要である。

3. EBM IRそのものをいじる

1.や2.で対応できない場合はEBM IRそのものをいじることになる。
この場合はEBMの構造が変わるため、ebm/extended_binary_module.bgnを編集し
update_ebm.pyを使って更新しebmgen/transform/内などに処理を追加する必要がある。
この方法はEBMの構造を変えることができるため柔軟な対応が可能である一方で、
EBMの構造が変わるため全体への影響が大きくなりがちであるため、変更の内容を十分に理解した上で行う必要がある。

## 注意点

上記のリファクタリングを行ったあとはregressionしていないかをunictest.pyで確認することが重要である。

**警告: `python script/unictest.py` を引数なしで実行するとPCが過負荷になるため厳禁。必ず `--target-runner ebm2<lang>` で言語を指定すること。**
例: `python script/unictest.py --target-runner ebm2zig`

指標としてはmainブランチの
https://on-keyday.github.io/brgen/unictest-results/test_results.json
と比較して、失敗数が増えていないことを確認する。

比較の手順 (ほぼ確実に何回か見ることになり毎回 curl すると遅いため、必ず save/ に落としてから解析すること):

```bash
mkdir -p save
curl -s "https://on-keyday.github.io/brgen/unictest-results/test_results.json" -o save/test_results_main.json

python -c "
import json
data = json.load(open('save/test_results_main.json'))
results = data['results']
runners = sorted(set(r['runner'] for r in results))
for runner in runners:
    rs = [r for r in results if r['runner'] == runner]
    passed = sum(1 for r in rs if r.get('success'))
    failed = [r['input_name'] for r in rs if not r.get('success')]
    print(f'{runner}: PASS={passed}, FAIL={len(failed)}, failed={failed}')
"
```

注意: runner の列挙はスクリプト内で自動的に行う。特定の言語名を決め打ちにしないこと（言語は増える可能性がある）。

特に頻発する例としてextended_binary_module_testが失敗することがあるがこれはtest/binary_data/extended_binary_module.datが更新されていない事によって起こりがちなのでまず他の原因を疑う前に
python script/update_ebm.pyを実行してtest/binary_data/extended_binary_module.datを更新してから再度試すこと。

### 1.固有の注意点

言語固有処理はhookファイルに分割されている場合とentry_before_class.hppにまとめられている場合があるため整理前に両方確認すること。

ctx.config()の中身はdefault_codegen_visitor/visitor/Visitor.hppと各言語固有のvisitor/Visitor_before.hppの両方で定義されているため両方確認すること。特にVisitor_before.hppの方は言語ごとに異なるため注意すること。

また言語数はCLAUDE.mdやSKILL.mdに書いてある時点から増えている可能性が高くまた、さらに増える可能性もあるため、そこに書いてある言語個数はあくまで参考程度にとどめて実際のディレクトリ構造とコードを確認すること。

ctx.config().\*\_visitorや　ctx.config().\*\_custom, ctx.config().\*\_wrapperの実体はdefault_codegen_visitor/visitor/Visitor.hppに存在しておりdefault_codegen_visitor/visitor/内のデフォルトフック内で呼び出されている。命名規則は以下の通り。

| サフィックス | 引数/戻り値の契約 /使い方                                                                                                                                                                                                                                                                                                                          |
| ------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `*_custom`   | `Context_*`クラスを引数に取り処理した場合は `expected<CodeWriter>` を返す。未処理の場合は `pass` を返してデフォルト処理に委ねる。`if(ctx.config().hoge_custom) { CALL_OR_PASS(dummy_name,ctx.config().hoge_custom(ctx)) }`という呼び出しパターンに従う。 新規に作るまたはリファクタリング時に置き換える場合は`*_visitor`よりかはこちらを推奨する。 |
| `*_visitor`  | 必ず `expected<CodeWriter>` を返す (完全オーバーライド)                                                                                                                                                                                                                                                                                            |
| `*_wrapper`  | `Context_*`とさらにデフォルトで処理されたいくつかの引数を受け取る。戻り値の契約は `*_visitor` と同じ。これの処理は各hookファイルごとに固有のカスタマイゼーションポイントとして実装がされる                                                                                                                                                         |
