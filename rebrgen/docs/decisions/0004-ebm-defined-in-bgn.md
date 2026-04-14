# EBM の定義を .bgn ファイルで行う

## 日付

2026-04-15

## 判断

EBM (Extended Binary Module) の IR フォーマット定義を extended_binary_module.bgn で記述し、
json2cpp2 で C++ のシリアライザ/デシリアライザを生成する。

## 動機

- **dogfooding**: brgen の言語自体をこういう用途にも使えることを示す実践。
  前身として json2vm/json2vm2 があり（定義は ../example/brgen_help/vm_enum.bgn, vm2.bgn 等）、
  ろくに動きはしなかったが、brgen の言語をヘルパ的な用途にも使えるという手応えがあった。
  この VM 構想が IR 構想の前身となり、IR を作る際にも同じ手法を応用した。
- **bootstrap 問題の回避**: src2json と json2cpp2 が既に動いていたため、
  それを有効活用する形で EBM の定義と C++ 実装の自動生成ができた。
- **本末転倒の回避**: brgen はそもそもシリアライザ/デシリアライザを手書きしたくないから
  作っているプロジェクト。IR の定義を C++ の struct で書いてシリアライザを手書きしたら
  本末転倒になる。

## 具体例

- `src/ebm/extended_binary_module.bgn` → json2cpp2 → `extended_binary_module.hpp/cpp`
- EBM の構造を変更したら `python script/update_ebm.py` で再生成するだけで済む

## これは X を意味しない

- brgen の言語が汎用プログラミング言語として十分だという主張ではない。
  バイナリフォーマット定義という用途に限れば十分に使えるという実証。

## 代替案

- C++ の struct で手書き定義 + 手書きシリアライザ: プロジェクトの存在意義と矛盾する。
