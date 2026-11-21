/*license*/
#pragma once
#include "lowering.hpp"

#include <vector>

// format の encode / decode に何を渡すか。
//
// `.bgn` に `fn decode` を書いていない format (as_is) には対応する関数ノードが
// 無いが、入れ子の format を読むときには呼ぶ先が要る。ebmgen は transform の
// `derive_encode_decode_wrapper` で関数宣言そのものを生やしているが、ここで
// 決められるのは **何を渡すか** までで、関数の名前も宣言の書き方も
// バックエンドの領分になる (生成コードの公開 API そのものなので)。
//
// 渡すもの:
//
//   ストリーム    decode なら入力、encode なら出力
//   state 変数    その方向でその format (と呼ぶ先) が読み書きするもの
//
// state は `Requirements` 表が呼び出しグラフ越しの不動点で持っている。
// ebmgen が呼び出しのたびに親子の `state_variables` を `ast_field` で
// 突き合わせているのと同じ情報で、こちらは先に集めてある。
//
// 失敗は値で返る。呼び先は失敗しうる値を返し、呼び元が検査して返す
// (ADR 0033: encode/decode 文脈の Assert は error を返す)。EBM ではこれが
// 型として実体化されていて (TypeKind::DECODER_RETURN / ENCODER_RETURN)、
// 綴りは言語ごとの値 knob (default visitor が config().decoder_return_type を
// 返すだけ)。nast にはまだその型が無い — 返す型は次に足すもの。
//
// 例外を持つ言語で例外に写すかはバックエンドの選択で、基本形は値返し。

namespace brgen::nast::lowering {

    enum class Direction {
        decode,
        encode,
    };

    struct CodecParams {
        // 読み書きの相手。decode なら input、encode なら output。
        Node<Expr> stream;
        // その方向で要る state 変数。読むだけのものと書くものを分けてある —
        // 書くほうは借用の仕方が言語ごとに違う (ADR 0034)。
        std::vector<Node<StateVariable>> read;
        std::vector<Node<StateVariable>> write;
    };

    // format が持たない (Requirements が無い) 場合も、ストリームだけは要るので
    // 空の state で返る。
    CodecParams codec_params(Context& c, Node<Format> fmt, Direction dir);

}  // namespace brgen::nast::lowering
