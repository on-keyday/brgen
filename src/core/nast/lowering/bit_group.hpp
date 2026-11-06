/*license*/
#pragma once
#include "lowering.hpp"

#include <span>
#include <vector>

// バイト境界に乗らない field の並び (ビット群) を 1 つの整数としてまとめて
// 読み書きする。EBM の merge_bit_field に当たる。
//
// ここで畳むのは **同じ Body の中でバイト境界から始まりバイト境界で閉じる群**
// だけ (wirespec の BitGroup と同じ条件)。閉じる前に分岐・Body の終わり・
// 可変幅に当たる群は、経路を辿るか format の境界を越えないと閉じないので
// 組めない (`nast_probe bits` が閉じ方ごとに数える。example/ では 508 群中
// 350 が平ら)。
//
// 群は 1 つの整数として読み、msb なら先頭の field を上位に、lsb なら下位に
// 置く。順の決め方は bit_group.cpp の order_of (bit_order が無ければバイト順
// に従う対角、2 バイト以上の非対角は組まない)。
//
// 群の途中にある読み書きしない文 (assert / 定義 / config) は群を閉じない。
// 群はまとめて読むので、それらは群全体を読んだ後 (書く側は書く前) に置く
// ことになる。置くのは呼ぶ側。

namespace brgen::nast::lowering {

    struct BitGroup {
        enum class Close {
            flat,            // 同じ Body の中でバイト境界に閉じた
            branch,          // 閉じる前に分岐 (If / Match) に当たった
            other_statement, // 閉じる前に分岐以外の読み書きする文に当たった
            variable_width,  // 閉じる前に幅の決まらない field に当たった
            body_end,        // 閉じる前に Body が終わった
        };
        Close close = Close::flat;
        std::vector<Node<Field>> fields;
        std::size_t bits = 0;
    };

    // Body の中のビット群を並び順に返す。閉じなかった群も閉じ方つきで返す。
    // バイト境界に乗る field が単独で並んでいるところは群にならない。
    std::vector<BitGroup> bit_groups(Context& c, Node<Body> body);

    // 組めない理由。組めるなら null。下の 2 つが null を返すのはこれが
    // null でないとき (と、targets の数が合わないとき) だけ。
    const char* bit_group_refusal(Context& c, const BitGroup& g);

    // 群をまとめて読む。targets は fields と同じ並びの書き先で、null なら
    // その field は読み捨てる。
    //
    //   bits<id> := <群全体を整数として読んだ値>
    //   f0 = <T0>((bits<id> >> s0) & m0)
    //   ...
    //
    // 組めなければ null。理由は bit_group_refusal が返す。
    Node<Body> lower_bit_group_decode(Context& c, const BitGroup& g, std::span<const Node<Expr>> targets,
                                      Node<Expr> bytes, Node<Expr> offset);

    // 群をまとめて書く。target が null の field は期待値があればそれを、
    // 無ければ 0 を書く。
    //
    //   <(<U>(f0) & m0) << s0 | ... を整数として書く>
    Node<Body> lower_bit_group_encode(Context& c, const BitGroup& g, std::span<const Node<Expr>> targets,
                                      Node<Expr> bytes, Node<Expr> offset);

}  // namespace brgen::nast::lowering
