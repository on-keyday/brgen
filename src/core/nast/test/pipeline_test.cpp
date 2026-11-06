/*license*/
// 解析と lowering と条件式が、testdata の .bgn に対して期待どおりに動くか。
//
// **期待値はファイルに置かず、ここに書く。** 綴りを golden ファイルに落とすと
// 更新の手間が本体より大きくなり、意図した変更と壊れた変更が同じ diff に
// 見える。ここでは「この式はこう綴られるはず」を assert として直に書く。
//
// nast_test (node/ の意味論) と往復試験 (parse -> wire/unparse -> 復元) の
// 間が空いていたので、その間を埋める: bind の結果、lowering の結果、
// query の条件式。
//
//   nast_pipeline_test <testdata ディレクトリ>
#include "../bind/pipeline.h"
#include "../lowering/available.hpp"
#include "../lowering/bit_group.hpp"
#include "../lowering/enum_defined.hpp"
#include "../lowering/field_io.hpp"
#include "../lowering/self_ref.hpp"
#include "../node/build.h"
#include "../node/console.h"
#include "../node/traverse.h"
#include "../node/util.h"
#include "../parse/unparse.h"
#include "../query/filter.hpp"
#include "../query/session.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace brgen::nast;

namespace {

    int failures = 0;

    void check(bool ok, const std::string& what) {
        print_line("  [{}] {}", ok ? "ok" : "NG", what);
        if (!ok) {
            failures++;
        }
    }

    void check_eq(const std::string& got, const std::string& want, const std::string& what) {
        check(got == want, what);
        if (got != want) {
            print_line("      期待 {}", want);
            print_line("      実際 {}", got);
        }
    }

    // その種のノードを順に。
    template <class T, class F>
    void each(Program& p, F&& fn) {
        each_node<T>(p.arena, [&](Node<T> n) { fn(n); });
    }

    // 名前で field を引く。testdata は小さいので線形で足りる。
    Node<Field> field_named(Program& p, std::string_view name) {
        Node<Field> found;
        each<Field>(p, [&](Node<Field> f) {
            if (!found && name_of(p.arena, f) == name) {
                found = f;
            }
        });
        return found;
    }

    std::string load(Program& p, const std::filesystem::path& dir, const char* name) {
        auto path = (dir / name).generic_string();
        auto r = analyze(p, path);
        if (r != AnalyzeResult::ok) {
            check(false, std::string(name) + " を解析できた");
        }
        return path;
    }

    // ---- bind/receiver ---------------------------------------------------

    void test_receiver(const std::filesystem::path& dir) {
        print_line("bind/receiver (receiver.bgn)");
        Program p;
        load(p, dir, "receiver.bgn");
        auto& a = p.arena;

        // format の field を指す参照だけがレシーバを取る。
        std::size_t with_receiver = 0;
        each<MemberAccess>(p, [&](Node<MemberAccess> ma) {
            if (ma.ref(a)->base.as_any<Self>()) {
                with_receiver++;
            }
        });
        check(with_receiver == 3, std::format("field 参照 3 つにレシーバが付く (実際 {})", with_receiver));

        // local / bound / mode は field ではないので裸のまま。
        for (auto name : {"local", "bound", "mode"}) {
            bool bare = false;
            each<Reference>(p, [&](Node<Reference> r) {
                if (ident_text(a, r.ref(a)->name) == name) {
                    bare = true;
                }
            });
            check(bare, std::string(name) + " は裸の参照のまま");
        }

        // 綴りは既定では原文どおり、self を頼めばレシーバも出る。
        auto fmt = field_named(p, "k").ref(a)->belong.as_any<Format>();
        auto plain = unparse_node(a, fmt);
        auto shown = unparse_node(a, fmt, UnparseOption{.explicit_self = true});
        check(plain.find("if k == 1:") != std::string::npos, "既定の綴りはレシーバを出さない");
        check(shown.find("if self.k == 1:") != std::string::npos, "explicit_self でレシーバが出る");
    }

    // ---- self キーワード -------------------------------------------------

    void test_self_keyword(const std::filesystem::path& dir) {
        print_line("parse/self (self_keyword.bgn)");
        Program p;
        load(p, dir, "self_keyword.bgn");
        auto& a = p.arena;

        // 原文の self は 2 つ、どちらも format の field を指す。
        std::size_t explicit_self = 0;
        std::size_t to_field = 0;
        each<MemberAccess>(p, [&](Node<MemberAccess> ma) {
            auto self = ma.ref(a)->base.as_any<Self>();
            if (!self || !self.ref(a)->is_explicit) {
                return;
            }
            explicit_self++;
            if (auto* r = p.tables.table<Resolution>().get(ma.ref(a)->member);
                r && r->target.as_any<Field>()) {
                to_field++;
            }
        });
        check(explicit_self == 2, std::format("self.len が 2 つ (実際 {})", explicit_self));
        check(to_field == 2, "self.x は field に解決する (同名の local には隠されない)");

        // 同名の local は VariableDefinition のまま。
        bool local_is_var = false;
        each<Reference>(p, [&](Node<Reference> r) {
            if (ident_text(a, r.ref(a)->name) != "len") {
                return;
            }
            if (auto* res = p.tables.table<Resolution>().get(r.ref(a)->name);
                res && res->target.as_any<VariableDefinition>()) {
                local_is_var = true;
            }
        });
        check(local_is_var, "裸の len は local を指す");
    }

    // ---- lowering --------------------------------------------------------

    void test_available(const std::filesystem::path& dir) {
        print_line("lowering/available (available.bgn)");
        Program p;
        load(p, dir, "available.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};

        std::map<std::string, std::string> got;
        each<Available>(p, [&](Node<Available> av) {
            auto e = lowering::lower_available(c, av);
            got[unparse_node(a, av)] = e ? unparse_node(a, e) : "(組めない)";
        });

        check_eq(got["available(value)"],
                 "(kind == 1) || (kind == 2)",
                 "裸の available は分岐の条件の畳み込み");
        check_eq(got["available(value,u16)"],
                 "!(kind == 1) && (kind == 2)",
                 "型を訊く形は候補の型で分ける");
        check_eq(got["available(c)"],
                 "(a == 1) && (b == 2)",
                 "入れ子の分岐は掛け合わせる");
        // 修飾された target は WithReceiver で包み、綴りは base 側に載る。
        check_eq(got["available(opt.value)"],
                 "(opt.kind == 1) || (opt.kind == 2)",
                 "修飾された target はレシーバを載せ替える");
    }

    void test_enum_defined(const std::filesystem::path& dir) {
        print_line("lowering/enum_defined (enum_defined.bgn)");
        Program p;
        load(p, dir, "enum_defined.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};

        std::string got;
        each<MemberAccess>(p, [&](Node<MemberAccess> ma) {
            if (auto e = lowering::lower_enum_is_defined(c, ma)) {
                got = unparse_node(a, e);
            }
        });
        check_eq(got, "((kind == Kind.A) || (kind == Kind.B)) || (kind == Kind.C)",
                 "is_defined は各メンバとの比較の連鎖");
    }

    void test_field_io(const std::filesystem::path& dir) {
        print_line("lowering/field_io (available.bgn)");
        Program p;
        load(p, dir, "available.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};

        // u16 の field を 1 つ取り、読み書きの綴りを見る。
        Node<Field> u16;
        each<Field>(p, [&](Node<Field> f) {
            auto t = strip_wrappers(a, f.ref(a)->type).as_any<IntType>();
            if (!u16 && t && t.ref(a)->bit_size == 16) {
                u16 = f;
            }
        });
        check(bool(u16), "u16 の field がある");
        if (!u16) {
            return;
        }
        Builder b{a, u16.ref(a).loc()};
        auto buf = b.ref("buf");
        auto off = b.ref("o");
        auto target = lowering::field_ref(c, u16);
        auto dec = lowering::lower_field_decode(c, u16, target, buf, off);
        check(bool(dec), "decode が組める");
        if (dec) {
            check_eq(unparse_node(a, dec.ref(a)->statements[0]),
                     "value = (u16(buf[o]) << 8) | u16(buf[o + 1])",
                     "u16 はバイトから合成する");
        }
    }

    // ビット群の切り方 (閉じ方) と、組んだ読み書きの綴り。
    void test_bit_group(const std::filesystem::path& dir) {
        print_line("lowering/bit_group (bit_group.bgn)");
        Program p;
        load(p, dir, "bit_group.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};
        using Close = lowering::BitGroup::Close;

        // format の名前で、その Body の群を引く。
        auto groups_of = [&](std::string_view name) {
            std::vector<lowering::BitGroup> r;
            each<Format>(p, [&](Node<Format> f) {
                if (name_of(a, f) == name) {
                    r = lowering::bit_groups(c, f.ref(a)->body);
                }
            });
            return r;
        };
        auto lines_of = [&](Node<Body> body) {
            std::vector<std::string> lines;
            if (body) {
                for (auto& st : body.ref(a)->statements) {
                    lines.push_back(unparse_node(a, st));
                }
            }
            return lines;
        };
        struct Built {
            std::vector<std::string> dec, enc;
            std::string why;
        };
        auto build = [&](const lowering::BitGroup& g) {
            std::vector<Node<Expr>> targets;
            for (auto f : g.fields) {
                targets.push_back(lowering::field_ref(c, f));
            }
            Builder b{a, g.fields.front().ref(a).loc()};
            Built r;
            r.dec = lines_of(lowering::lower_bit_group_decode(c, g, targets, b.ref("buf"), b.ref("o")));
            r.enc = lines_of(lowering::lower_bit_group_encode(c, g, targets, b.ref("buf"), b.ref("o")));
            auto why = lowering::bit_group_refusal(c, g);
            r.why = why ? why : "";
            return r;
        };
        auto bits_name = [&](const lowering::BitGroup& g) {
            return derived_name("bits", g.fields.front());
        };

        {
            auto gs = groups_of("OneByte");
            check(gs.size() == 1 && gs[0].close == Close::flat && gs[0].bits == 8, "OneByte: 8 bit の平らな群 1 つ");
            if (gs.size() == 1) {
                auto r = build(gs[0]);
                auto v = bits_name(gs[0]);
                check(r.dec.size() == 5 && r.enc.size() == 3, "OneByte: decode 5 行 / encode 3 行");
                if (r.dec.size() == 5) {
                    check_eq(r.dec[0], v + " := u8(buf[o])", "1 バイトはそのまま読む");
                    check_eq(r.dec[1], std::format("kind = <Nib>(({} >> 4) & 15)", v), "先頭の field が上位、enum へは cast");
                    check_eq(r.dec[3], "reserved == 0", "期待値の検査は読んだ直後");
                    check_eq(r.dec[4], std::format("flag = <u1>({} & 1)", v), "最下位はシフトしない");
                }
                if (r.enc.size() == 3) {
                    check_eq(r.enc[0], "reserved == 0", "期待値の検査は書く前");
                    check_eq(r.enc[2], std::format("buf[o] = u8({})", v), "まとめた値を 1 度だけ書く");
                }
            }
        }
        {
            auto gs = groups_of("Straddle");
            check(gs.size() == 1 && gs[0].fields.size() == 3 && gs[0].bits == 16, "Straddle: バイト幅の field を挟んで 16 bit");
            if (gs.size() == 1) {
                auto r = build(gs[0]);
                if (r.dec.size() == 4) {
                    check_eq(r.dec[2], std::format("mid = <u8>(({} >> 4) & 255)", bits_name(gs[0])), "挟まった u8 も切り出す");
                }
                else {
                    check(false, "Straddle: decode 4 行");
                }
            }
        }
        {
            auto gs = groups_of("Exactly64");
            check(gs.size() == 1 && gs[0].bits == 64, "Exactly64: 64 bit");
            if (gs.size() == 1) {
                auto r = build(gs[0]);
                check_eq(r.why, "", "64 bit ちょうどは組める");
                if (r.dec.size() == 3) {
                    auto v = bits_name(gs[0]);
                    check_eq(r.dec[1], std::format("top = <u1>(({} >> 63) & 1)", v), "最上位ビット");
                    check_eq(r.dec[2], std::format("rest = <u63>({} & 9223372036854775807)", v), "63 bit の mask");
                }
                else {
                    check(false, "Exactly64: decode 3 行");
                }
            }
        }
        {
            auto gs = groups_of("Over64");
            check(gs.size() == 1 && gs[0].bits == 72, "Over64: 72 bit の群");
            if (gs.size() == 1) {
                auto r = build(gs[0]);
                check_eq(r.why, "64 ビットを超える", "Over64 は組めない");
                check(r.dec.empty() && r.enc.empty(), "Over64: 何も組まない");
            }
        }
        {
            auto gs = groups_of("Signed");
            check(gs.size() == 1 && gs[0].close == Close::flat, "Signed: 平らな群");
            if (gs.size() == 1) {
                check_eq(build(gs[0]).why, "符号つき", "符号つきは組めない");
            }
        }
        {
            auto gs = groups_of("Branch");
            check(gs.size() == 1 && gs[0].close == Close::branch && gs[0].bits == 2, "Branch: 閉じる前に分岐");
            if (gs.size() == 1) {
                check_eq(build(gs[0]).why, "平らでない", "分岐で閉じない群は組めない");
            }
            // Branch の Body にある If の腕だけを見る (他の format にも If がある)。
            std::size_t arm_end = 0;
            each<Format>(p, [&](Node<Format> f) {
                if (name_of(a, f) != "Branch") {
                    return;
                }
                for (auto& st : f.ref(a)->body.ref(a)->statements) {
                    auto n = st.as_any<If>();
                    if (!n) {
                        continue;
                    }
                    for (auto& blk : n.ref(a)->blocks) {
                        for (auto& g : lowering::bit_groups(c, blk.ref(a)->body)) {
                            arm_end += g.close == Close::body_end;
                        }
                    }
                }
            });
            check(arm_end == 2, "Branch: 腕は 2 つとも閉じる前に Body が終わる");
        }
        {
            auto gs = groups_of("Half");
            check(gs.size() == 1 && gs[0].close == Close::body_end && gs[0].bits == 4, "Half: 閉じる前に format が終わる");
        }
        {
            auto gs = groups_of("Little");
            check(gs.size() == 2, "Little: 群が 2 つ");
            if (gs.size() == 2) {
                auto one = build(gs[0]);
                check_eq(one.why, "", "リトルの 1 バイトは組める");
                if (one.dec.size() == 3) {
                    auto v = bits_name(gs[0]);
                    check_eq(one.dec[1], std::format("a = <u4>({} & 15)", v), "リトルは先頭の field が下位");
                    check_eq(one.dec[2], std::format("b = <u4>(({} >> 4) & 15)", v), "2 つ目はその上");
                }
                else {
                    check(false, "Little 1 バイト: decode 3 行");
                }
                auto two = build(gs[1]);
                check_eq(two.why, "", "リトルの 2 バイトは対角 (LE×LSB) なので組める");
                if (two.dec.size() == 3) {
                    auto v = bits_name(gs[1]);
                    check_eq(two.dec[0], std::format("{} := (u16(buf[o + 1]) << 8) | u16(buf[o])", v), "整数はリトルで読む (上位は後ろのバイト)");
                    check_eq(two.dec[1], std::format("c = <u4>({} & 15)", v), "先頭が下位");
                    check_eq(two.dec[2], std::format("d = <u12>(({} >> 4) & 4095)", v), "12 bit はその上");
                }
                else {
                    check(false, "Little 2 バイト: decode 3 行");
                }
            }
        }
        {
            auto gs = groups_of("BigLsb");
            check(gs.size() == 2, "BigLsb: 群が 2 つ");
            if (gs.size() == 2) {
                auto one = build(gs[0]);
                check_eq(one.why, "", "1 バイトは bit_order だけで決まる");
                if (one.dec.size() == 3) {
                    check_eq(one.dec[1], std::format("a = <u2>({} & 3)", bits_name(gs[0])), "lsb なら先頭が下位");
                }
                else {
                    check(false, "BigLsb 1 バイト: decode 3 行");
                }
                check_eq(build(gs[1]).why, "2 バイト以上でバイト順とビット順が対角でない", "BE×LSB の 2 バイトは組めない");
            }
        }
        {
            auto gs = groups_of("DynamicBitOrder");
            check(gs.size() == 1, "DynamicBitOrder: 群が 1 つ");
            if (gs.size() == 1) {
                check_eq(build(gs[0]).why, "bit_order が実行時に決まる", "実行時の bit_order は組めない");
            }
        }
    }

    // 分岐を跨ぐ群。経路で調べて、位置が静的なものを組む。
    void test_routed_bit_group(const std::filesystem::path& dir) {
        print_line("lowering/bit_group の分岐を跨ぐ群 (bit_group.bgn)");
        Program p;
        load(p, dir, "bit_group.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};

        // format の Body と、その最初の field の位置。
        auto routed = [&](std::string_view name, std::string& why) -> std::optional<lowering::RoutedBitGroup> {
            Node<Body> body;
            each<Format>(p, [&](Node<Format> f) {
                if (name_of(a, f) == name) {
                    body = f.ref(a)->body;
                }
            });
            if (!body) {
                why = "format が無い";
                return std::nullopt;
            }
            auto& stmts = body.ref(a)->statements;
            std::size_t start = 0;
            while (start < stmts.size() && !stmts[start].as<Field>()) {
                start++;
            }
            const char* w = nullptr;
            auto g = lowering::routed_bit_group(c, body, start, &w);
            why = w ? w : "";
            return g;
        };
        auto text = [&](Node<Body> body) {
            std::vector<std::string> lines;
            if (body) {
                for (auto& st : body.ref(a)->statements) {
                    lines.push_back(unparse_node(a, st));
                }
            }
            return lines;
        };
        auto field_at = [&](const lowering::RoutedBitGroup& g, std::size_t i) {
            return std::format("{}@{}:{} 読む前 [{},{}) 書いた後 [{},{})", name_of(a, g.fields[i].field), g.fields[i].offset,
                               g.fields[i].bits, g.fields[i].fetch_from, g.fields[i].fetch_to, g.fields[i].flush_from,
                               g.fields[i].flush_to);
        };
        auto lower = [&](const lowering::RoutedBitGroup& g, std::size_t i, bool decode) {
            Builder b{a, g.fields[i].field.ref(a).loc()};
            auto target = lowering::field_ref(c, g.fields[i].field);
            return text(decode ? lowering::lower_routed_bit_decode(c, g, i, target, b.ref("buf"), b.ref("o"))
                               : lowering::lower_routed_bit_encode(c, g, i, target, b.ref("buf"), b.ref("o")));
        };

        std::string why;
        if (auto g = routed("VarIntLike", why)) {
            check(g->fields.size() == 5 && g->max_bytes == 8 && !g->lsb, "VarIntLike: prefix と腕の v 4 つ、最長 8 バイト");
            if (g->fields.size() == 5) {
                check_eq(field_at(*g, 0), "prefix@0:2 読む前 [0,1) 書いた後 [0,0)", "prefix は 1 バイト目を読ませる");
                check_eq(field_at(*g, 4), "v@2:62 読む前 [1,8) 書いた後 [0,8)", "62 bit の腕は残り 7 バイトを読ませる");
                auto dec = lower(*g, 0, true);
                check(dec.size() == 1 && dec[0] == "prefix = <u2>((u8(buf[o]) >> 6) & 3)", "prefix は上位 2 bit");
                auto enc = lower(*g, 2, false);
                check(enc.size() == 3 && enc[1] == "buf[o] = buf[o] | u8(" + derived_name("bits", g->fields[2].field) + " >> 8)",
                      "書く側は 0 で埋めたバイトへ OR で重ねる");
            }
        }
        else {
            check(false, "VarIntLike が組める: " + why);
        }
        if (auto g = routed("ArmThenTail", why)) {
            check(g->fields.size() == 4, "ArmThenTail: flag / x / y / tail");
            if (g->fields.size() == 4) {
                check_eq(field_at(*g, 3), "tail@4:4 読む前 [1,1) 書いた後 [0,1)", "腕から戻った tail は位置 4");
                auto dec = lower(*g, 3, true);
                check(dec.size() == 1 && dec[0] == "tail = <u4>(u8(buf[o]) & 15)", "tail は下位 4 bit");
            }
        }
        else {
            check(false, "ArmThenTail が組める: " + why);
        }
        check(!routed("NoElse", why) && why == "位置が経路で変わる", "NoElse: else が無いと tail の位置が変わる");
        {
            // 入出力に触れる定義は群を閉じる。触れないものは通す。
            auto closes = [&](std::string_view name) {
                std::vector<lowering::BitGroup::Close> r;
                each<Format>(p, [&](Node<Format> f) {
                    if (name_of(a, f) == name) {
                        for (auto& g : lowering::bit_groups(c, f.ref(a)->body)) {
                            r.push_back(g.close);
                        }
                    }
                });
                return r;
            };
            using Close = lowering::BitGroup::Close;
            auto io = closes("IoInBetween");
            check(io.size() == 2 && io[0] == Close::other_statement, "IoInBetween: input.get() の定義で群が閉じる");
            auto pure = closes("PureInBetween");
            check(pure.size() == 1 && pure[0] == Close::flat, "PureInBetween: 入力に触れない定義は群を閉じない");
        }
        {
            // ループの Body から始まる群。break の経路は閉じない。
            Node<Body> loop_body;
            each<Loop>(p, [&](Node<Loop> l) {
                auto& st = l.ref(a)->body.ref(a)->statements;
                if (!st.empty() && st[0].as<Field>() && name_of(a, st[0]) == "a") {
                    loop_body = l.ref(a)->body;
                }
            });
            check(bool(loop_body), "BreakInside: ループの Body がある");
            if (loop_body) {
                auto routes = lowering::bit_routes(c, loop_body, 0);
                std::size_t open = 0;
                for (auto& r : routes) {
                    open += r.end != lowering::BitRoute::End::closed;
                }
                check(routes.size() == 2 && open == 1, "BreakInside: break の経路だけ閉じない");
                const char* w = nullptr;
                check(!lowering::routed_bit_group(c, loop_body, 0, &w) && w && std::string(w) == "閉じない経路がある",
                      "BreakInside: 群ごと断る");
            }
        }
        if (auto g = routed("RoutedLittle", why)) {
            check(g->lsb, "RoutedLittle: lsb から");
            if (g->fields.size() == 3) {
                auto dec = lower(*g, 2, true);
                check(dec.size() == 1 && dec[0] == "b = <u12>((((u16(buf[o + 1]) << 8) | u16(buf[o])) >> 4) & 4095)",
                      "lsb の 2 バイトはリトルで読み、先頭の 4 bit の上から切る");
            }
            else {
                check(false, "RoutedLittle: sel / a / b");
            }
        }
        else {
            check(false, "RoutedLittle が組める: " + why);
        }
    }

    // `f :T(期待値)` が読み書きの前後に assert として付くか。読む側は読んで
    // から、書く側は書く前 (lowering/field_io.hpp)。名前つきの引数は組めない。
    void test_field_expected(const std::filesystem::path& dir) {
        print_line("lowering/field_io の期待値 (field_io.bgn)");
        Program p;
        load(p, dir, "field_io.bgn");
        auto& a = p.arena;
        lowering::Context c{a, p.tables};

        auto lower = [&](std::string_view name, bool decode) -> std::vector<std::string> {
            auto f = field_named(p, name);
            if (!f) {
                check(false, std::format("{} がある", name));
                return {};
            }
            Builder b{a, f.ref(a).loc()};
            auto target = lowering::field_ref(c, f);
            auto body = decode ? lowering::lower_field_decode(c, f, target, b.ref("buf"), b.ref("o"))
                               : lowering::lower_field_encode(c, f, target, b.ref("buf"), b.ref("o"));
            std::vector<std::string> lines;
            if (body) {
                for (auto& st : body.ref(a)->statements) {
                    lines.push_back(unparse_node(a, st));
                }
            }
            return lines;
        };

        auto dec = lower("magic", true);
        check_eq(dec.empty() ? "" : dec.back(), "magic == 0xcafe",
                 "読む側は読んだ後に検査する");
        auto enc = lower("magic", false);
        check_eq(enc.empty() ? "" : enc.front(), "magic == 0xcafe",
                 "書く側は書く前に検査する");

        auto tag = lower("tag", true);
        check_eq(tag.empty() ? "" : tag.back(), "tag == Kind.b",
                 "enum の期待値は enum のまま比べる");

        // 配列にスカラの期待値が付いたら、比べるのは要素のほう。
        auto zeros = lower("zeros", true);
        check(zeros.size() == 1, "配列は要素を回す 1 文になる");
        auto loop = zeros.empty() ? std::string() : zeros.front();
        check(loop.find("] == 0") != std::string::npos, "検査は要素ごとに入る");
        check(loop.find("zeros == 0") == std::string::npos, "配列そのものとは比べない");

        check(lower("body", true).empty(), "名前つきの引数が付いた field は組めない");
        check(!lower("len", true).empty(), "引数の無い field はそのまま組める");
    }

    // ---- query -----------------------------------------------------------

    void test_query(const std::filesystem::path& dir) {
        print_line("query (self_keyword.bgn)");
        Program p;
        load(p, dir, "self_keyword.bgn");
        query::Session s{p};

        auto count = [&](const char* text, std::optional<NodeType> kind) -> std::size_t {
            std::string err;
            auto f = query::parse_filter(text, err);
            if (!f) {
                check(false, std::string("条件式が読める: ") + text + " (" + err + ")");
                return 0;
            }
            return s.select(kind, f).size();
        };

        check(count("base.$kind == Self", NodeType::MemberAccess) == 2,
              "$kind でノード種を比べられる");
        check(count("$line == 9 and $kind == Ident", std::nullopt) == 2,
              "擬似フィールドは Any にも効く");
        // 実フィールドとしての kind は Ident に無いので当たらない (前は擬似に
        // 落ちて全件当たっていた)。
        check(count("kind == Ident", std::nullopt) == 0,
              "印の無い名前は木のフィールドだけを引く");
        check(count("name.identifier == \"len\"", NodeType::Field) == 1,
              "文字列は引用符ありでも比べられる");
        // self.len の 2 つ。宣言側の Ident は表に載らない。
        check(count("@Resolution.target.$kind == Field", NodeType::Ident) == 2,
              "side table を辿れる");

        std::string err;
        check(!query::parse_filter("$", err) && !err.empty(), "壊れた条件式は理由つきで落ちる");
    }

    // ---- 解析の全体像 ----------------------------------------------------

    void test_corpus(const std::filesystem::path& dir) {
        print_line("解析 (testdata 全部)");
        std::size_t files = 0;
        std::size_t untyped = 0;
        std::size_t unresolved = 0;
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() != ".bgn") {
                continue;
            }
            Program p;
            if (analyze(p, entry.path().generic_string()) != AnalyzeResult::ok) {
                check(false, entry.path().filename().generic_string() + " を解析できた");
                continue;
            }
            files++;
            unresolved += p.stats.names_unresolved;
            // 木から辿れる式はすべて型が付くはず (testdata は意図的な穴を
            // 含まない)。**アリーナを舐めない** — パーサが先読みして捨てた
            // ノードが残っていて、そちらは型付けの対象ではない。
            for (auto& mod : p.modules) {
                visit_all(p.arena, mod, [&](NodeAny n) {
                    if (auto e = n.as_any<Expr>(); e && !e.ref(p.arena)->type) {
                        untyped++;
                    }
                    return true;
                });
            }
        }
        check(files > 0, std::format("testdata を {} 本読んだ", files));
        check(unresolved == 0, std::format("未解決の名前が無い (実際 {})", unresolved));
        check(untyped == 0, std::format("型の付かない式が無い (実際 {})", untyped));
    }

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_line(stderr, "usage: nast_pipeline_test <testdata ディレクトリ>");
        return 2;
    }
    std::filesystem::path dir = argv[1];
    test_receiver(dir);
    test_self_keyword(dir);
    test_available(dir);
    test_enum_defined(dir);
    test_field_io(dir);
    test_field_expected(dir);
    test_bit_group(dir);
    test_routed_bit_group(dir);
    test_query(dir);
    test_corpus(dir);
    print_line("");
    print_line("{}", failures ? std::format("{} 件 NG", failures) : "全部 ok");
    return failures ? 1 : 0;
}
