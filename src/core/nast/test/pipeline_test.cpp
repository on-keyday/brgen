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
                 "(kind == 1) ? true : ((kind == 2) ? true : false)",
                 "裸の available は分岐の条件の畳み込み");
        check_eq(got["available(value,u16)"],
                 "(kind == 1) ? false : ((kind == 2) ? true : false)",
                 "型を訊く形は候補の型で分ける");
        check_eq(got["available(c)"],
                 "(a == 1) ? ((b == 2) ? true : false) : false",
                 "入れ子の分岐は掛け合わせる");
        // 修飾された target は WithReceiver で包み、綴りは base 側に載る。
        check_eq(got["available(opt.value)"],
                 "(opt.kind == 1) ? true : ((opt.kind == 2) ? true : false)",
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
    test_query(dir);
    test_corpus(dir);
    print_line("");
    print_line("{}", failures ? std::format("{} 件 NG", failures) : "全部 ok");
    return failures ? 1 : 0;
}
