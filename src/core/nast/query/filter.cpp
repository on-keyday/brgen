/*license*/
#include "filter.hpp"
#include "comb2/internal/test.h"
#include "comb2/status.h"
#include "session.hpp"
#include "../node/printer.h"
#include "../node/traverse.h"
#include "../parse/unparse.h"

#include <comb2/basic/group.h>
#include <comb2/composite/number.h>
#include <comb2/composite/range.h>
#include <comb2/composite/string.h>
#include <comb2/lexctx.h>

#include <charconv>
#include <format>

namespace brgen::nast::query {

    namespace {

        // ---- 字句 ------------------------------------------------------------

        // 字句は comb2 で組む。brgen の字句解析器 (src/core/lexer/lexer.h) と
        // ebmgen の debugger が同じ書き方をしているので、綴りの規則を
        // 手書きの走査で書き直さない。構文のほうは nast の parse/ と同じく
        // 手書きの再帰下降。
        // comb2 の演算子は自由関数なので、外に出すと範囲 for の `*it` や `-x`
        // まで拾ってしまう。字句の定義だけを囲って閉じ込める
        // (lexer.h が internal 名前空間でそうしているのと同じ理由)。
        namespace lex {

            namespace cps = futils::comb2::composite;
            using namespace futils::comb2::ops;

            enum class Tok {
                space,
                number,
                string,
                ident,
                sym,
            };

            constexpr auto tok_space = str(Tok::space, ~(cps::space | cps::tab));
            constexpr auto tok_number = str(Tok::number, cps::hex_integer | cps::dec_integer);
            constexpr auto tok_string = str(Tok::string, cps::c_str);
            constexpr auto tok_ident = str(Tok::ident, cps::c_ident);
            // 長いものから見る (`==` が `=` に、`->` が `-` に食われないように)。
            constexpr auto tok_sym = str(Tok::sym, lit("==") | lit("!=") | lit("<=") | lit(">=") |
                                                       lit("&&") | lit("||") | lit("->") | lit("<") |
                                                       lit(">") | lit("!") | lit("(") | lit(")") |
                                                       lit("[") | lit("]") | lit(".") | lit("@") |
                                                       lit("$"));
            constexpr auto tok_one = tok_space | tok_number | tok_string | tok_ident | tok_sym;

            constexpr auto test() {
                auto a_parse = [](auto x) {
                    auto seq = futils::make_ref_seq(x);
                    return tok_one(seq, futils::comb2::test::TestContext<Tok>{}, 0);
                };
                return a_parse("Module");
            }

            static_assert(test() == futils::comb2::Status::match, "Must parse");

            struct Token {
                Tok kind = Tok::sym;
                std::string text;
                std::size_t pos = 0;
            };

            // 読む口。構文側は「合えば進む」だけを使う。
            struct Lexer {
                std::vector<Token> toks;
                std::size_t at = 0;
                // 誤りの位置は文字の位置で言う (トークン番号では原文を指せない)。
                std::size_t pos = 0;

                bool eof() const {
                    return at >= toks.size();
                }

                const Token* peek() const {
                    return eof() ? nullptr : &toks[at];
                }

                void advance() {
                    at++;
                    pos = eof() ? pos : toks[at].pos;
                }

                // 記号か、その綴りの語 (and / or / not は識別子として来る)。
                bool sym(std::string_view s) {
                    auto* t = peek();
                    if (!t || (t->kind != Tok::sym && t->kind != Tok::ident) || t->text != s) {
                        return false;
                    }
                    advance();
                    return true;
                }

                std::string ident() {
                    auto* t = peek();
                    if (!t || t->kind != Tok::ident) {
                        return {};
                    }
                    auto text = t->text;
                    advance();
                    return text;
                }

                std::string number() {
                    auto* t = peek();
                    if (!t || t->kind != Tok::number) {
                        return {};
                    }
                    auto text = t->text;
                    advance();
                    return text;
                }

                // 引用符は落として返す。
                bool string_lit(std::string& out) {
                    auto* t = peek();
                    if (!t || t->kind != Tok::string) {
                        return false;
                    }
                    out = t->text.size() >= 2 ? t->text.substr(1, t->text.size() - 2) : std::string();
                    advance();
                    return true;
                }
            };

            bool tokenize(std::string_view src, Lexer& lx, std::string& err) {
                auto seq = futils::make_ref_seq(src);
                while (!seq.eos()) {
                    auto ctx = futils::comb2::LexContext<Tok, std::string>{};
                    if (auto status = tok_one(seq, ctx, 0); status != futils::comb2::Status::match) {
                        auto substr = src.substr(seq.rptr);
                        err = std::format("読めない字がある {} at {} {} (len: {})", status == futils::comb2::Status::fatal ? "fatal" : "not_match", seq.rptr, substr, substr.size());
                        return false;
                    }
                    auto pos = ctx.str_pos;
                    if (ctx.str_tag == Tok::space) {
                        continue;
                    }
                    lx.toks.push_back(Token{ctx.str_tag,
                                            std::string(src.substr(pos.begin, pos.end - pos.begin)),
                                            pos.begin});
                }
                lx.pos = lx.toks.empty() ? 0 : lx.toks.front().pos;
                return true;
            }

        }  // namespace lex

        using lex::Lexer;
        using lex::tokenize;

        std::optional<std::int64_t> as_int(std::string_view s) {
            if (s.empty()) {
                return std::nullopt;
            }
            std::int64_t v = 0;
            int base = 10;
            if (s.starts_with("0x") || s.starts_with("0X")) {
                base = 16;
                s = s.substr(2);
            }
            auto* first = s.data();
            auto* last = s.data() + s.size();
            auto [ptr, ec] = std::from_chars(first, last, v, base);
            if (ec != std::errc{} || ptr != last) {
                return std::nullopt;
            }
            return v;
        }

        std::string_view unquote(std::string_view s) {
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
                return s.substr(1, s.size() - 2);
            }
            return s;
        }

        // ---- 値 --------------------------------------------------------------

        struct Val {
            bool ok = false;
            bool is_node = false;
            bool is_list = false;
            std::uint32_t id = 0;
            NodeType type{};
            std::string text;
            std::vector<std::pair<std::uint32_t, NodeType>> list;
        };

        Val node_val(std::uint32_t id, NodeType t) {
            return Val{.ok = id != 0, .is_node = true, .id = id, .type = t};
        }

        Val text_val(std::string s) {
            return Val{.ok = true, .text = std::move(s)};
        }

    }  // namespace

    // ---- 条件木 --------------------------------------------------------------

    enum class CmpOp { none,
                       eq,
                       ne,
                       lt,
                       le,
                       gt,
                       ge };

    struct Step {
        enum class Kind { field,
                          pseudo,
                          index,
                          table } kind = Kind::field;
        std::string name;
        std::size_t index = 0;
    };

    struct Filter {
        enum class Kind { or_,
                          and_,
                          not_,
                          test } kind = Kind::test;
        std::vector<FilterPtr> kids;
        std::vector<Step> path;
        CmpOp op = CmpOp::none;
        std::string literal;
    };

    namespace {

        // ---- 構文 ------------------------------------------------------------

        struct Parser {
            Lexer lx;
            std::string err;

            bool fail(std::string_view msg) {
                if (err.empty()) {
                    err = std::format("{} (位置 {})", msg, lx.pos);
                }
                return false;
            }

            // 名前の前の印で、どこを引くかが決まる。印が無ければ木の
            // フィールド、`@` は side table、`$` はノード自身について訊く
            // 擬似フィールド。同じ名前が両方にあっても (`SpecialLiteral.kind`)
            // 隠れない。
            bool step_name(std::vector<Step>& out) {
                if (lx.sym("@")) {
                    auto name = lx.ident();
                    if (name.empty()) {
                        return fail("@ の後に表の名前が要る");
                    }
                    out.push_back(Step{Step::Kind::table, name, 0});
                    return true;
                }
                if (lx.sym("$")) {
                    auto name = lx.ident();
                    if (name.empty()) {
                        return fail("$ の後に擬似フィールドの名前が要る");
                    }
                    out.push_back(Step{Step::Kind::pseudo, name, 0});
                    return true;
                }
                auto name = lx.ident();
                if (name.empty()) {
                    return fail("フィールド名が要る");
                }
                out.push_back(Step{Step::Kind::field, name, 0});
                return true;
            }

            bool path(std::vector<Step>& out) {
                if (!step_name(out)) {
                    return false;
                }
                for (;;) {
                    if (lx.sym("->") || lx.sym(".")) {
                        if (lx.sym("$")) {
                            auto name = lx.ident();
                            if (name.empty()) {
                                return fail("$ の後に擬似フィールドの名前が要る");
                            }
                            out.push_back(Step{Step::Kind::pseudo, name, 0});
                            continue;
                        }
                        auto name = lx.ident();
                        if (name.empty()) {
                            return fail("`.` の後にフィールド名が要る");
                        }
                        out.push_back(Step{Step::Kind::field, name, 0});
                        continue;
                    }
                    if (lx.sym("[")) {
                        auto digits = lx.number();
                        auto n = as_int(digits);
                        if (!n || *n < 0) {
                            return fail("`[` の中は番号");
                        }
                        if (!lx.sym("]")) {
                            return fail("`]` が無い");
                        }
                        out.push_back(Step{Step::Kind::index, {}, std::size_t(*n)});
                        continue;
                    }
                    break;
                }
                return true;
            }

            bool test(FilterPtr& out) {
                auto e = std::make_shared<Filter>();
                e->kind = Filter::Kind::test;
                if (!path(e->path)) {
                    return false;
                }
                struct {
                    const char* sym;
                    CmpOp op;
                } ops[] = {{"==", CmpOp::eq}, {"!=", CmpOp::ne}, {"<=", CmpOp::le}, {">=", CmpOp::ge}, {"<", CmpOp::lt}, {">", CmpOp::gt}};
                for (auto& o : ops) {
                    if (lx.sym(o.sym)) {
                        e->op = o.op;
                        break;
                    }
                }
                if (e->op != CmpOp::none) {
                    std::string lit;
                    if (!lx.string_lit(lit)) {
                        // 数・名前 (種別名や列挙の綴り) のどちらでも書ける。
                        lit = lx.number();
                        if (lit.empty()) {
                            lit = lx.ident();
                        }
                        if (lit.empty()) {
                            return fail("比較する値が要る");
                        }
                    }
                    e->literal = std::move(lit);
                }
                out = e;
                return true;
            }

            bool unary(FilterPtr& out) {
                if (lx.sym("!") || lx.sym("not")) {
                    FilterPtr inner;
                    if (!unary(inner)) {
                        return false;
                    }
                    auto e = std::make_shared<Filter>();
                    e->kind = Filter::Kind::not_;
                    e->kids.push_back(inner);
                    out = e;
                    return true;
                }
                if (lx.sym("(")) {
                    if (!expr(out)) {
                        return false;
                    }
                    if (!lx.sym(")")) {
                        return fail("`)` が無い");
                    }
                    return true;
                }
                return test(out);
            }

            // 同じ演算子の連なりは 1 つのノードにまとめる。
            bool chain(FilterPtr& out, Filter::Kind kind, auto&& sub, std::initializer_list<const char*> syms) {
                FilterPtr first;
                if (!sub(first)) {
                    return false;
                }
                std::vector<FilterPtr> kids{first};
                for (;;) {
                    bool hit = false;
                    for (auto s : syms) {
                        if (lx.sym(s)) {
                            hit = true;
                            break;
                        }
                    }
                    if (!hit) {
                        break;
                    }
                    FilterPtr next;
                    if (!sub(next)) {
                        return false;
                    }
                    kids.push_back(next);
                }
                if (kids.size() == 1) {
                    out = kids.front();
                    return true;
                }
                auto e = std::make_shared<Filter>();
                e->kind = kind;
                e->kids = std::move(kids);
                out = e;
                return true;
            }

            bool and_(FilterPtr& out) {
                return chain(out, Filter::Kind::and_, [&](FilterPtr& o) { return unary(o); }, {"and", "&&"});
            }

            bool expr(FilterPtr& out) {
                return chain(out, Filter::Kind::or_, [&](FilterPtr& o) { return and_(o); }, {"or", "||"});
            }
        };

        // ---- 評価 ------------------------------------------------------------

        // どのノードにもある擬似フィールド。木のフィールドより先に見る。
        bool pseudo_field(const Session& s, std::uint32_t id, const std::string& name, Val& out) {
            auto* h = s.p.arena.header_at(id);
            if (!h) {
                return false;
            }
            if (name == "id") {
                out = text_val(std::to_string(id));
                return true;
            }
            if (name == "kind") {
                out = text_val(to_string(h->type));
                return true;
            }
            if (name == "line") {
                out = text_val(std::to_string(h->loc.line));
                return true;
            }
            if (name == "col") {
                out = text_val(std::to_string(h->loc.col));
                return true;
            }
            if (name == "orphan") {
                out = text_val(s.reachable.contains(id) ? "false" : "true");
                return true;
            }
            if (name == "text") {
                out = text_val(unparse_node(const_cast<Arena&>(s.p.arena), s.node_at(id)));
                return true;
            }
            return false;
        }

        Val field_of_node(const Session& s, std::uint32_t id, const std::string& name) {
            Val out;
            auto* h = s.p.arena.header_at(id);
            if (!h) {
                return {};
            }
            auto& a = const_cast<Arena&>(s.p.arena);
            auto data_index = h->data_index;
            visit_node_type(h->type, [&](auto tag) {
                using T = typename decltype(tag)::type;
                auto* d = a.template data_at<T>(data_index);
                if (!d) {
                    return;
                }
                d->for_each_field([&](const char* field, const auto& v, bool) {
                    if (name != field) {
                        return;
                    }
                    using M = std::decay_t<decltype(v)>;
                    if constexpr (node_of<M>::is_node) {
                        out = node_val(v.id(), v.type());
                    }
                    else if constexpr (vector_of<M>::is_vector) {
                        out.ok = true;
                        out.is_list = true;
                        for (auto& e : v) {
                            out.list.push_back({e.id(), e.type()});
                        }
                    }
                    else {
                        out = text_val(scalar_text(v));
                    }
                });
            });
            return out;
        }

        // side table を引く。`@Resolution` だけなら「在るか」、続けて
        // フィールド名があればその中身。
        Val table_of_node(const Session& s, std::uint32_t id, const std::string& table_name,
                          const std::string* field) {
            auto* h = s.p.arena.header_at(id);
            if (!h) {
                return {};
            }
            Val out;
            s.p.tables.for_each_table([&](const char* name, const auto& table) {
                if (table_name != name) {
                    return;
                }
                using table_t = std::decay_t<decltype(table)>;
                using key_node = Node<typename table_t::node_type>;
                auto key = key_node::from_unique_id((std::uint64_t(h->type) << 32) | id);
                if (!table.contains(key)) {
                    return;
                }
                if (!field) {
                    out = text_val("true");
                    return;
                }
                if constexpr (requires { table.get(key); }) {
                    if (const auto* entry = table.get(key)) {
                        entry->for_each_field([&](const char* f, const auto& v, bool) {
                            if (*field != f) {
                                return;
                            }
                            using M = std::decay_t<decltype(v)>;
                            if constexpr (node_of<M>::is_node) {
                                out = node_val(v.id(), v.type());
                            }
                            else if constexpr (vector_of<M>::is_vector) {
                                out.ok = true;
                                out.is_list = true;
                                for (auto& e : v) {
                                    out.list.push_back({e.id(), e.type()});
                                }
                            }
                            else {
                                out = text_val(scalar_text(v));
                            }
                        });
                    }
                }
            });
            return out;
        }

        Val eval_path(const Session& s, std::uint32_t id, const std::vector<Step>& path) {
            Val cur = node_val(id, s.p.arena.header_at(id)->type);
            for (std::size_t i = 0; i < path.size(); i++) {
                auto& st = path[i];
                if (st.kind == Step::Kind::table) {
                    if (!cur.is_node) {
                        return {};
                    }
                    // 表の中身は 1 段だけ。`@表.フィールド` をまとめて解く。
                    const std::string* field = nullptr;
                    if (i + 1 < path.size() && path[i + 1].kind == Step::Kind::field) {
                        field = &path[i + 1].name;
                        i++;
                    }
                    cur = table_of_node(s, cur.id, st.name, field);
                }
                else if (st.kind == Step::Kind::pseudo) {
                    if (!cur.is_node) {
                        return {};
                    }
                    Val got;
                    if (!pseudo_field(s, cur.id, st.name, got)) {
                        return {};
                    }
                    cur = got;
                }
                else if (st.kind == Step::Kind::index) {
                    if (!cur.is_list || st.index >= cur.list.size()) {
                        return {};
                    }
                    auto [nid, ntype] = cur.list[st.index];
                    cur = node_val(nid, ntype);
                }
                else if (cur.is_list) {
                    // 並びに対して書けるのは長さだけ。
                    if (st.name != "length") {
                        return {};
                    }
                    cur = text_val(std::to_string(cur.list.size()));
                }
                else if (cur.is_node) {
                    cur = field_of_node(s, cur.id, st.name);
                }
                else {
                    return {};  // スカラーの先は無い
                }
                if (!cur.ok) {
                    return {};
                }
            }
            return cur;
        }

        bool compare_text(std::string_view l, CmpOp op, std::string_view r) {
            if (auto li = as_int(l), ri = as_int(r); li && ri) {
                switch (op) {
                    case CmpOp::eq:
                        return *li == *ri;
                    case CmpOp::ne:
                        return *li != *ri;
                    case CmpOp::lt:
                        return *li < *ri;
                    case CmpOp::le:
                        return *li <= *ri;
                    case CmpOp::gt:
                        return *li > *ri;
                    case CmpOp::ge:
                        return *li >= *ri;
                    default:
                        return false;
                }
            }
            switch (op) {
                case CmpOp::eq:
                    return l == r;
                case CmpOp::ne:
                    return l != r;
                case CmpOp::lt:
                    return l < r;
                case CmpOp::le:
                    return l <= r;
                case CmpOp::gt:
                    return l > r;
                case CmpOp::ge:
                    return l >= r;
                default:
                    return false;
            }
        }

        bool eval(const Session& s, std::uint32_t id, const Filter& e) {
            switch (e.kind) {
                case Filter::Kind::or_:
                    for (auto& k : e.kids) {
                        if (eval(s, id, *k)) {
                            return true;
                        }
                    }
                    return false;
                case Filter::Kind::and_:
                    for (auto& k : e.kids) {
                        if (!eval(s, id, *k)) {
                            return false;
                        }
                    }
                    return true;
                case Filter::Kind::not_:
                    return !eval(s, id, *e.kids.front());
                case Filter::Kind::test:
                    break;
            }
            auto v = eval_path(s, id, e.path);
            if (!v.ok) {
                return false;
            }
            if (e.op == CmpOp::none) {
                // 比較を書かない path は「辿れたか」。false という値は偽にする。
                return !v.is_node || v.id != 0 ? unquote(v.text) != "false" : false;
            }
            auto lit = unquote(e.literal);
            if (v.is_list) {
                return false;  // 並びそのものは比べられない (.length を使う)
            }
            if (v.is_node) {
                // 数で書かれたら id、名前で書かれたら種別。
                if (as_int(lit)) {
                    return compare_text(std::to_string(v.id), e.op, lit);
                }
                return compare_text(to_string(v.type), e.op, lit);
            }
            return compare_text(unquote(v.text), e.op, lit);
        }

    }  // namespace

    FilterPtr parse_filter(std::string_view text, std::string& err) {
        Parser ps;
        if (!tokenize(text, ps.lx, err)) {
            return nullptr;
        }
        FilterPtr out;
        if (!ps.expr(out)) {
            err = ps.err;
            return nullptr;
        }
        if (!ps.lx.eof()) {
            err = std::format("読み切れない: {}", text.substr(ps.lx.pos));
            return nullptr;
        }
        return out;
    }

    bool matches(const Session& s, std::uint32_t id, const Filter& e) {
        if (!s.p.arena.header_at(id)) {
            return false;
        }
        return eval(s, id, e);
    }

}  // namespace brgen::nast::query
