/*license*/
#pragma once
#include "lowering.hpp"

#include <map>
#include <optional>
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

    // 分岐を跨ぐ群の経路。群の先頭から、木を辿って各経路がバイト境界で
    // 閉じるまで進めたもの (EBM の search_byte_aligned_route に当たる)。
    // nast の制御は構造化されている (If / Match / Loop だけ) ので、CFG を
    // 作らずに木の上で数え上げる。
    //
    // 分岐はそれぞれの腕へ分かれ、既定の腕 (else / `..`) が無ければ
    // 「何も読まずに分岐の後へ進む」経路も足す。腕の終わりで開いていれば
    // 分岐の後へ戻って続ける。
    struct BitRoute {
        enum class End {
            closed,           // バイト境界で閉じた
            variable_width,   // 閉じる前に幅の決まらない field
            loop,             // 閉じる前にループ
            other_statement,  // 閉じる前に読み書きする分岐以外の文
            body_end,         // 閉じる前に最初の Body が終わった
            too_long,         // 64 ビットを超えた
        };
        End end = End::closed;
        std::vector<Node<Field>> fields;
        std::size_t bits = 0;
        // 閉じた経路の続き。閉じた位置から先に読む文の場所を内側の Body から
        // 順に。閉じた後にまた群の field を読む経路を見つけるのに使う。
        struct Resume {
            Node<Body> body;
            std::size_t index = 0;
        };
        std::vector<Resume> rest;
    };

    // body の statements[start] (群の先頭の field) から経路を数え上げる。
    std::vector<BitRoute> bit_routes(Context& c, Node<Body> body, std::size_t start);

    // 分岐を跨ぐ群のうち、どの field も経路によらず群の先頭から同じビット位置
    // にあるもの。位置が静的なので、その field を読む前に要るバイトも、書いた
    // 後に書き終わるバイトも静的に決まる — 実行時のカーソルが要らない
    // (EBM の multi_route_merged は位置を実行時に持つ)。
    //
    // バイトを持つのは呼ぶ側。field ごとに、読む前に bytes へ揃えておく範囲と、
    // 書いた後に出してよい範囲を返す。範囲は群の先頭からのバイト番号。
    struct RoutedBitField {
        Node<Field> field;
        std::size_t offset = 0;  // 群の先頭からのビット位置
        std::size_t bits = 0;
        std::size_t fetch_from = 0, fetch_to = 0;  // 読む前に揃えておくバイト
        std::size_t flush_from = 0, flush_to = 0;  // 書いた後に書き終わるバイト
    };
    struct RoutedBitGroup {
        std::vector<RoutedBitField> fields;  // 位置の順
        std::size_t max_bytes = 0;           // 一番長い経路のバイト数
        bool lsb = false;                    // 先頭のビットを下位に置くか
    };

    // body の statements[start] から始まる群を経路で調べて組む。組めなければ
    // 空を返し、理由を why に置く。
    std::optional<RoutedBitGroup> routed_bit_group(Context& c, Node<Body> body, std::size_t start, const char** why);

    // fields[index] を bytes から読む。bytes[offset + fetch_from .. fetch_to) は
    // 呼ぶ側が先に読んでおくこと。
    Node<Body> lower_routed_bit_decode(Context& c, const RoutedBitGroup& g, std::size_t index, Node<Expr> target,
                                       Node<Expr> bytes, Node<Expr> offset);

    // fields[index] を bytes へ OR で重ねる。bytes は群の前に 0 で埋めておく
    // こと。書いた後 bytes[offset + flush_from .. flush_to) を出してよい。
    // target が null なら期待値を、それも無ければ何も重ねない (0 のまま)。
    Node<Body> lower_routed_bit_encode(Context& c, const RoutedBitGroup& g, std::size_t index, Node<Expr> target,
                                       Node<Expr> bytes, Node<Expr> offset);

    // format の中の field がどの群に属すか。バックエンドが field ごとに引く表。
    //
    // format の Body を先頭から歩き、バイト境界にいる所でバイト境界に乗らない
    // field に当たったら、そこから群を作る: 同じ Body の中で閉じれば平らな群、
    // 閉じる前に分岐に当たれば分岐を跨ぐ群。群に入った field は (腕の中や
    // 分岐の後にあっても) その群の一員として載り、歩く側はもう触らない。
    //
    // 組めない群の field は理由つきで載る。群が閉じないと、その後の位置は
    // 静的に分からない — 同じ Body の残りと、それを囲む Body の残りの field は
    // (バイト幅でも) 「手前の群が閉じない」で載る。平らな群が組めないだけ
    // (符号つきなど) なら閉じてはいるので、後ろの位置は分かる。
    //
    // 載らない field はバイト境界から読む普通の field。
    struct BitPlace {
        enum class Kind {
            flat,     // flat[group].fields[member]
            routed,   // routed[group].fields[member]
            refused,  // why に理由
        };
        Kind kind = Kind::refused;
        std::size_t group = 0;
        std::size_t member = 0;
        const char* why = nullptr;
    };
    struct BitLayout {
        std::vector<BitGroup> flat;
        std::vector<RoutedBitGroup> routed;
        std::map<std::uint32_t, BitPlace> places;  // field の id から

        const BitPlace* find(Node<Field> f) const;
        // 組めない field が 1 つも無い。
        bool complete() const;
    };

    // fmt の Body (入れ子の format の中は含まない) の表を作る。
    BitLayout bit_layout(Context& c, Node<Format> fmt);

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
