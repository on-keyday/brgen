/*license*/
#include "unparse.h"

#include "../node/util.h"

namespace brgen::nast {

    namespace {

        // enum の綴り (演算子の記号など)。enum_array は {値, 綴り} の対。
        template <class E>
        std::string_view spell(E v) {
            for (auto& [e, s] : enum_array<E>) {
                if (e == v) {
                    return s;
                }
            }
            return "?";
        }

        struct Unparser {
            Arena& a;
            CodeWriter w;
            IndentStack indents{w};

            // 行を終える。次の write は新しい行に載り、インデントは Writer が
            // 現在の深さから決める。
            void nl() {
                w.line();
            }

            void enter() {
                indents.enter();
            }

            void leave() {
                indents.leave();
            }

            // 名前が無いときは、消えるより目印が残るほうがよい (未実装や
            // 壊れた入力を出力から見つけられる)。
            std::string_view ident_text(Node<Ident> id) {
                auto text = nast::ident_text(a, id);
                return text.empty() ? std::string_view("$missing") : text;
            }

            // ---- 式 ------------------------------------------------------

            void arguments(Node<Arguments> ar) {
                auto d = ar.ref(a);
                if (!d) {
                    return;
                }
                bool first = true;
                for (auto& arg : d->arguments) {
                    if (!first) {
                        w.write(", ");
                    }
                    first = false;
                    if (auto na = arg.as_any<NamedArgument>()) {
                        expr(na.ref(a)->name);
                        w.write(" = ");
                    }
                    expr(arg.ref(a)->value);
                }
            }

            // <u8>(x) と u8(x) は同じ Cast になる。中の型リテラルの is_explicit
            // (parse_type を通ったか) で書き分ける。組み込み名でない型は
            // 裸では書けないので常に <> で書く。
            void type_literal(Node<TypeLiteral> tl) {
                auto t = tl.ref(a)->literal;
                bool bare_ok = t.as_any<IntType>() || t.as_any<FloatType>() ||
                               t.as_any<BoolType>() || t.as_any<VoidType>();
                if (bare_ok && !t.ref(a)->is_explicit) {
                    type(t);
                    return;
                }
                w.write("<");
                type(t);
                w.write(">");
            }

            void escape_into(std::string_view s) {
                std::string escaped;
                for (char c : s) {
                    if (c == '\\' || c == '"') {
                        escaped += '\\';
                    }
                    escaped += c;
                }
                w.write(escaped);
            }

            // 式と文の入口では、書いた範囲をそのノードに紐づけておく。
            // 入れ子は内側ほど狭い範囲として重なって記録される。
            void expr(Node<Expr> e) {
                if (!e) {
                    w.write("/*missing*/");
                    return;
                }
                auto scope = w.with_loc_scope(e);
                expr_body(e);
            }

            void expr_body(Node<Expr> e) {
                switch (e.type()) {
                    case NodeType::IntLiteral:
                        w.write(e.as<IntLiteral>().ref(a)->value);
                        return;
                    case NodeType::BoolLiteral:
                        w.write(e.as<BoolLiteral>().ref(a)->value ? "true" : "false");
                        return;
                    case NodeType::StrLiteral:
                        w.write(e.as<StrLiteral>().ref(a)->value);
                        return;
                    case NodeType::CharLiteral:
                        w.write(e.as<CharLiteral>().ref(a)->value);
                        return;
                    case NodeType::RegexLiteral:
                        w.write(e.as<RegexLiteral>().ref(a)->value);
                        return;
                    case NodeType::SpecialLiteral:
                        w.write(to_string(e.as<SpecialLiteral>().ref(a)->kind, true));
                        return;
                    case NodeType::Reference:
                        w.write(ident_text(e.as<Reference>().ref(a)->name));
                        return;
                    case NodeType::MemberAccess: {
                        auto d = e.as<MemberAccess>().ref(a);
                        // 実体化したレシーバ (bind/receiver) は原文に無いので
                        // 綴らない。原文に `self` と書かれていたものは綴る
                        // (`is_explicit`)。Cast の `<u8>(x)` / `u8(x)` と同じ、
                        // 「同じノードに畳んだ 2 つの書き方」の区別。
                        auto self = d->base.as_any<Self>();
                        if (!self || self.ref(a)->is_explicit) {
                            expr(d->base);
                            w.write(".");
                        }
                        w.write(ident_text(d->member));
                        return;
                    }
                    case NodeType::Index: {
                        auto d = e.as<Index>().ref(a);
                        expr(d->base);
                        w.write("[");
                        expr(d->index);
                        w.write("]");
                        return;
                    }
                    case NodeType::Call: {
                        auto d = e.as<Call>().ref(a);
                        expr(d->callee);
                        w.write("(");
                        arguments(d->arguments);
                        w.write(")");
                        return;
                    }
                    case NodeType::Cast: {
                        auto d = e.as<Cast>().ref(a);
                        auto call = d->base.ref(a);
                        if (auto tl = call->callee.as_any<TypeLiteral>()) {
                            type_literal(tl);
                        }
                        else {
                            expr(call->callee);
                        }
                        w.write("(");
                        arguments(d->arguments);
                        w.write(")");
                        return;
                    }
                    case NodeType::TypeLiteral:
                        type_literal(e.as<TypeLiteral>());
                        return;
                    case NodeType::Paren: {
                        w.write("(");
                        auto before = w.line_count();
                        expr(e.as<Paren>().ref(a)->expr);
                        // 中身が if 式などでブロックを開いていたら、閉じ括弧は
                        // 次の行に置かないと再 parse できない。
                        if (w.line_count() != before) {
                            nl();
                        }
                        w.write(")");
                        return;
                    }
                    case NodeType::Unary: {
                        auto d = e.as<Unary>().ref(a);
                        w.write(spell(d->op));
                        expr(d->target);
                        return;
                    }
                    case NodeType::Binary: {
                        auto d = e.as<Binary>().ref(a);
                        expr(d->left);
                        if (d->op == BinaryOp::comma) {
                            w.write(", ");
                        }
                        else {
                            w.write(" ");
                            w.write(spell(d->op));
                            w.write(" ");
                        }
                        expr(d->right);
                        return;
                    }
                    case NodeType::Range: {
                        auto d = e.as<Range>().ref(a);
                        auto before = w.line_count();
                        if (d->start) {
                            expr(d->start);
                        }
                        // start が match / if でブロックを開いていたら演算子は
                        // 次の行へ。parse は文末の改行を読み飛ばした後の範囲
                        // 演算子を式の続きとして拾う (tree_test.bgn の `..=`)。
                        if (w.line_count() != before) {
                            nl();
                        }
                        w.write(spell(d->op));
                        if (d->end) {
                            expr(d->end);
                        }
                        return;
                    }
                    case NodeType::Cond: {
                        auto d = e.as<Cond>().ref(a);
                        expr(d->cond);
                        w.write(" ? ");
                        expr(d->then);
                        w.write(" : ");
                        expr(d->els);
                        return;
                    }
                    case NodeType::Available: {
                        auto av = e.as<Available>().ref(a);
                        w.write("available(");
                        expr(av->target);
                        if (av->selected_type) {
                            // `available(x, u8)` の型。落とすと往復で消える。
                            w.write(",");
                            type_literal(av->selected_type);
                        }
                        w.write(")");
                        return;
                    }
                    case NodeType::Sizeof:
                        w.write("sizeof(");
                        expr(e.as<Sizeof>().ref(a)->target);
                        w.write(")");
                        return;
                    case NodeType::Self:
                        // 綴りは .bgn の構文には無い。合成した木を印字した
                        // ときだけ出る。実際の綴りはバックエンドが決める。
                        w.write("self");
                        return;
                    case NodeType::BitCast: {
                        // 綴りは .bgn の構文には無い。合成した木を印字した
                        // ときだけ出る (is_little_endian と同じ)。行き先は
                        // この式自身の型。
                        auto d = e.as<BitCast>().ref(a);
                        w.write("bit_cast<");
                        type(d->type);
                        w.write(">(");
                        expr(d->target);
                        w.write(")");
                        return;
                    }
                    case NodeType::IsLittleEndian: {
                        // 綴りはバックエンドが決めるもので、.bgn の構文には
                        // 無い。ここに出るのは合成した木を印字したときだけ
                        // (木からは辿れず表からしか来ない)。再パースできない
                        // 唯一の綴りなのはそのため。
                        auto d = e.as<IsLittleEndian>().ref(a);
                        w.write("is_little_endian(");
                        if (d->order) {
                            expr(d->order.ref(a)->order);
                        }
                        w.write(")");
                        return;
                    }
                    case NodeType::BitSizeof:
                        w.write("bit_sizeof(");
                        expr(e.as<BitSizeof>().ref(a)->target);
                        w.write(")");
                        return;
                    case NodeType::OrCond:
                        // base が元のカンマ連結。conds は同じ葉を共有している。
                        expr(e.as<OrCond>().ref(a)->base);
                        return;
                    case NodeType::Identity:
                        expr(e.as<Identity>().ref(a)->expr);
                        return;
                    case NodeType::Import:
                        w.write("config.import(\"");
                        escape_into(e.as<Import>().ref(a)->path);
                        w.write("\")");
                        return;
                    case NodeType::If:
                        if_statement(e.as<If>());
                        return;
                    case NodeType::Match:
                        match_statement(e.as<Match>());
                        return;
                    default:
                        w.write("/*unprintable ");
                        w.write(to_string(e.type()));
                        w.write("*/");
                        return;
                }
            }

            // ---- 型 ------------------------------------------------------

            void type(Node<Type> t) {
                if (!t) {
                    w.write("/*missing type*/");
                    return;
                }
                switch (t.type()) {
                    case NodeType::IntType: {
                        auto d = t.as<IntType>().ref(a);
                        w.write(d->is_signed ? "i" : "u");
                        if (d->endian == Endian::big) {
                            w.write("b");
                        }
                        else if (d->endian == Endian::little) {
                            w.write("l");
                        }
                        w.write(std::to_string(d->bit_size));
                        return;
                    }
                    case NodeType::FloatType: {
                        auto d = t.as<FloatType>().ref(a);
                        w.write("f");
                        if (d->endian == Endian::big) {
                            w.write("b");
                        }
                        else if (d->endian == Endian::little) {
                            w.write("l");
                        }
                        w.write(std::to_string(d->bit_size));
                        return;
                    }
                    case NodeType::BoolType:
                        w.write("bool");
                        return;
                    case NodeType::VoidType:
                        w.write("void");
                        return;
                    case NodeType::IdentType:
                        w.write(ident_text(t.as<IdentType>().ref(a)->ident));
                        return;
                    case NodeType::ImportedType:
                        expr(t.as<ImportedType>().ref(a)->import_ref);
                        return;
                    case NodeType::ArrayType: {
                        auto d = t.as<ArrayType>().ref(a);
                        w.write("[");
                        if (d->length) {
                            expr(d->length);
                        }
                        w.write("]");
                        type(d->element_type);
                        return;
                    }
                    case NodeType::StrLiteralType: {
                        auto lit = t.as<StrLiteralType>().ref(a)->base.ref(a);
                        w.write(lit ? lit->value : "/*missing literal*/");
                        return;
                    }
                    case NodeType::RegexLiteralType: {
                        auto lit = t.as<RegexLiteralType>().ref(a)->base.ref(a);
                        w.write(lit ? lit->value : "/*missing literal*/");
                        return;
                    }
                    case NodeType::FunctionType: {
                        auto d = t.as<FunctionType>().ref(a);
                        w.write("fn (");
                        bool first = true;
                        for (auto& p : d->parameters) {
                            if (!first) {
                                w.write(", ");
                            }
                            first = false;
                            w.write(":");
                            type(p);
                        }
                        w.write(")");
                        if (d->return_type) {
                            w.write(" -> ");
                            type(d->return_type);
                        }
                        return;
                    }
                    case NodeType::GenericType: {
                        auto d = t.as<GenericType>().ref(a);
                        type(d->base_type);
                        w.write("[");
                        bool first = true;
                        for (auto& arg : d->type_arguments) {
                            if (!first) {
                                w.write(", ");
                            }
                            first = false;
                            type(arg);
                        }
                        w.write("]");
                        return;
                    }
                    // 宣言を指す型。原文には宣言の名前が書かれていて、この
                    // ノード自体は typer が合成する (EnumType は Enum の
                    // enum_type、StructType は format の struct_type)。lowering が
                    // これらへの cast を組むことがあるので、名前で綴る。
                    case NodeType::EnumType: {
                        if (auto base = t.as<EnumType>().ref(a)->base) {
                            w.write(ident_text(base.ref(a)->name));
                            return;
                        }
                        break;
                    }
                    case NodeType::StructType: {
                        auto base = t.as<StructType>().ref(a)->base;
                        if (auto fmt = base.as_any<Format>()) {
                            w.write(ident_text(fmt.ref(a)->name));
                            return;
                        }
                        break;
                    }
                    case NodeType::InlineStructType: {
                        auto fmt = t.as<InlineStructType>().ref(a)->inlined_format;
                        w.write("format");
                        if (auto name = fmt.ref(a)->name) {
                            w.write(" ");
                            w.write(ident_text(name));
                        }
                        w.write(":");
                        body(fmt.ref(a)->body);
                        return;
                    }
                    default:
                        w.write("/*unprintable type ");
                        w.write(to_string(t.type()));
                        w.write("*/");
                        return;
                }
            }

            // ---- 文 ------------------------------------------------------

            void body(Node<Body> b) {
                enter();
                auto d = b.ref(a);
                if (d) {
                    for (auto& s : d->statements) {
                        nl();
                        statement(s);
                    }
                }
                leave();
            }

            // match の `=> 文` に置けるか。ブロックを開く文はだめ。
            bool fits_on_a_line(Node<Statement> s) {
                switch (s.type()) {
                    case NodeType::If:
                    case NodeType::Match:
                    case NodeType::Loop:
                    case NodeType::RangeLoop:
                    case NodeType::Format:
                    case NodeType::GenericFormat:
                    case NodeType::Enum:
                    case NodeType::State:
                    case NodeType::Function:
                        return false;
                    default:
                        break;
                }
                // インライン format を型に持つ field もブロックを開く
                if (auto f = s.as_any<NamedTypeStatement>()) {
                    auto t = f.ref(a)->type;
                    while (auto arr = t.as_any<ArrayType>()) {
                        t = arr.ref(a)->element_type;
                    }
                    if (t.as_any<InlineStructType>()) {
                        return false;
                    }
                }
                return true;
            }

            void if_statement(Node<If> n) {
                auto d = n.ref(a);
                bool first = true;
                for (auto& block : d->blocks) {
                    if (auto cs = block.as_any<ConditionalStatement>()) {
                        if (first) {
                            w.write("if ");
                        }
                        else {
                            nl();
                            w.write("elif ");
                        }
                        expr(cs.ref(a)->condition);
                        w.write(":");
                    }
                    else {
                        nl();
                        w.write("else:");
                    }
                    first = false;
                    body(block.ref(a)->body);
                }
            }

            void match_statement(Node<Match> n) {
                auto d = n.ref(a);
                w.write("match");
                if (d->condition) {
                    w.write(" ");
                    expr(d->condition);
                }
                w.write(":");
                enter();
                for (auto& block : d->blocks) {
                    nl();
                    auto cs = block.as<ConditionalStatement>().ref(a);
                    expr(cs->condition);
                    auto b = cs->body.ref(a);
                    // `=> 文` と 1 文だけのブロックは同じ木。1 行に置ける文なら
                    // `=>` で書く。
                    if (b && b->statements.size() == 1 && fits_on_a_line(b->statements[0])) {
                        w.write(" => ");
                        statement(b->statements[0]);
                    }
                    else {
                        w.write(":");
                        body(cs->body);
                    }
                }
                leave();
            }

            void loop_statement(Node<Loop> n) {
                auto d = n.ref(a);
                w.write("for");
                if (!d->init && !d->condition && !d->step) {
                    w.write(":");
                }
                else {
                    // `for cond:` は init に条件そのもの (真偽演算子なら Assert 包み)
                    // が入る。init と condition が同じものを指していたらこの形。
                    bool cond_only = false;
                    if (d->init && d->condition && !d->step) {
                        if (d->init == d->condition) {
                            cond_only = true;
                        }
                        else if (auto asrt = d->init.as_any<Assert>()) {
                            cond_only = asrt.ref(a)->expr == d->condition;
                        }
                    }
                    w.write(" ");
                    if (cond_only) {
                        expr(d->condition);
                    }
                    else {
                        if (d->init) {
                            statement(d->init);
                        }
                        w.write(";");
                        if (d->condition) {
                            w.write(" ");
                            expr(d->condition);
                        }
                        if (d->step) {
                            w.write("; ");
                            statement(d->step);
                        }
                    }
                    w.write(":");
                }
                body(d->body);
            }

            void enum_statement(Node<Enum> n) {
                auto d = n.ref(a);
                w.write("enum ");
                w.write(ident_text(d->name));
                w.write(":");
                enter();
                if (d->base_type) {
                    nl();
                    w.write(":");
                    type(d->base_type);
                }
                for (auto& m : d->members) {
                    nl();
                    auto md = m.ref(a);
                    w.write(ident_text(md->name));
                    // 値は書かれなかったものも parse が合成する。書かれたもの
                    // (raw_expr) だけを書き、合成分は再 parse に作り直させる。
                    if (md->raw_expr) {
                        w.write(" = ");
                        expr(md->raw_expr);
                    }
                }
                leave();
            }

            void fn_statement(Node<Function> n) {
                auto d = n.ref(a);
                w.write("fn ");
                w.write(ident_text(d->name));
                w.write("(");
                bool first = true;
                for (auto& p : d->parameters) {
                    if (!first) {
                        w.write(", ");
                    }
                    first = false;
                    auto pd = p.ref(a);
                    if (pd->name) {
                        w.write(ident_text(pd->name));
                        w.write(" ");
                    }
                    w.write(":");
                    type(pd->type);
                }
                w.write(")");
                // 書かれなかった戻り値は parse が VoidType を合成する。合成分は
                // is_explicit が立たないのでそこで見分ける。
                if (d->return_type &&
                    !(d->return_type.as_any<VoidType>() && !d->return_type.ref(a)->is_explicit)) {
                    w.write(" -> ");
                    type(d->return_type);
                }
                w.write(":");
                body(d->body);
            }

            void field_statement(Node<NamedTypeStatement> n, Node<Arguments> args) {
                auto d = n.ref(a);
                if (d->name) {
                    w.write(ident_text(d->name));
                    w.write(" ");
                }
                w.write(":");
                type(d->type);
                if (args) {
                    w.write("(");
                    arguments(args);
                    w.write(")");
                }
            }

            void statement(Node<Statement> s) {
                if (!s) {
                    w.write("/*missing statement*/");
                    return;
                }
                auto scope = w.with_loc_scope(s);
                statement_body(s);
            }

            void statement_body(Node<Statement> s) {
                switch (s.type()) {
                    case NodeType::Format: {
                        auto d = s.as<Format>().ref(a);
                        w.write("format ");
                        w.write(ident_text(d->name));
                        w.write(":");
                        body(d->body);
                        return;
                    }
                    case NodeType::GenericFormat: {
                        auto d = s.as<GenericFormat>().ref(a);
                        w.write("format ");
                        w.write(ident_text(d->name));
                        w.write("[");
                        bool first = true;
                        for (auto& tp : d->type_parameters) {
                            if (!first) {
                                w.write(", ");
                            }
                            first = false;
                            w.write(ident_text(tp.ref(a)->name));
                        }
                        w.write("]:");
                        body(d->body);
                        return;
                    }
                    case NodeType::State: {
                        auto d = s.as<State>().ref(a);
                        w.write("state ");
                        w.write(ident_text(d->name));
                        w.write(":");
                        body(d->body);
                        return;
                    }
                    case NodeType::Enum:
                        enum_statement(s.as<Enum>());
                        return;
                    case NodeType::Function:
                        fn_statement(s.as<Function>());
                        return;
                    case NodeType::Field:
                        field_statement(s.as<Field>(), s.as<Field>().ref(a)->arguments);
                        return;
                    case NodeType::StateVariable:
                        field_statement(s.as<StateVariable>(), s.as<StateVariable>().ref(a)->arguments);
                        return;
                    case NodeType::If:
                        if_statement(s.as<If>());
                        return;
                    case NodeType::Match:
                        match_statement(s.as<Match>());
                        return;
                    case NodeType::Loop:
                        loop_statement(s.as<Loop>());
                        return;
                    case NodeType::RangeLoop: {
                        auto d = s.as<RangeLoop>().ref(a);
                        auto bind = d->binding.ref(a);
                        w.write("for ");
                        w.write(ident_text(bind->name));
                        w.write(" in ");
                        expr(bind->value);
                        w.write(":");
                        body(d->body);
                        return;
                    }
                    case NodeType::Return: {
                        auto d = s.as<Return>().ref(a);
                        w.write("return");
                        if (d->expr) {
                            w.write(" ");
                            expr(d->expr);
                        }
                        return;
                    }
                    case NodeType::Break:
                        w.write("break");
                        return;
                    case NodeType::Continue:
                        w.write("continue");
                        return;
                    case NodeType::Assert:
                        // 真偽演算子の文は parse が Assert に包む。式だけ書けば
                        // 再 parse が包み直す。
                        expr(s.as<Assert>().ref(a)->expr);
                        return;
                    case NodeType::Assign: {
                        auto d = s.as<Assign>().ref(a);
                        expr(d->assignee);
                        w.write(" ");
                        w.write(spell(d->op));
                        w.write(" ");
                        expr(d->value);
                        return;
                    }
                    case NodeType::VariableDefinition: {
                        auto d = s.as<VariableDefinition>().ref(a);
                        w.write(ident_text(d->name));
                        w.write(" ");
                        w.write(spell(d->op));
                        w.write(" ");
                        expr(d->value);
                        return;
                    }
                    case NodeType::Metadata: {
                        auto d = s.as<Metadata>().ref(a);
                        w.write(d->name);
                        auto args = d->arguments.ref(a);
                        // 代入形と 1 引数の呼び出し形は同じ木。代入形で書く。
                        if (args && args->arguments.size() == 1) {
                            w.write(" = ");
                            expr(args->arguments[0].ref(a)->value);
                        }
                        else {
                            w.write("(");
                            arguments(d->arguments);
                            w.write(")");
                        }
                        return;
                    }
                    case NodeType::SpecifyOrder: {
                        auto d = s.as<SpecifyOrder>().ref(a);
                        w.write(d->name);
                        w.write(" = ");
                        expr(d->order);
                        return;
                    }
                    case NodeType::ExplicitError: {
                        // extra_arguments は message も含む元の引数列そのもの。
                        w.write("error(");
                        arguments(s.as<ExplicitError>().ref(a)->extra_arguments);
                        w.write(")");
                        return;
                    }
                    default:
                        if (auto e = s.as_any<Expr>()) {
                            expr(e);
                            return;
                        }
                        w.write("/*unprintable statement ");
                        w.write(to_string(s.type()));
                        w.write("*/");
                        return;
                }
            }
        };

    }  // namespace

    namespace {

        void run_module(Unparser& u, Node<Module> mod) {
            auto d = mod.ref(u.a);
            if (!d) {
                return;
            }
            bool first = true;
            for (auto& s : d->statements) {
                if (!first) {
                    u.nl();
                }
                first = false;
                u.statement(s);
            }
            u.nl();  // 末尾の改行
        }

        // どのノードから始めるかを、種類を見て振り分ける。Module は文の並びで
        // 末尾に改行が要るが、それ以外は 1 つ書くだけ。Expr は Statement でも
        // あるので先に見る (式の位置での書き方が要るため)。
        void run_any(Unparser& u, NodeAny n) {
            if (!n) {
                return;
            }
            if (auto mod = n.as_any<Module>()) {
                run_module(u, mod);
                return;
            }
            if (auto e = n.as_any<Expr>()) {
                u.expr(e);
                return;
            }
            if (auto s = n.as_any<Statement>()) {
                u.statement(s);
                return;
            }
            if (auto t = n.as_any<Type>()) {
                u.type(t);
                return;
            }
            if (auto id = n.as_any<Ident>()) {
                u.w.write(u.ident_text(id));
                return;
            }
        }

    }  // namespace

    std::string unparse(Arena& a, Node<Module> mod) {
        return unparse_node(a, mod);
    }

    CodeOutput unparse_with_spans(Arena& a, Node<Module> mod) {
        return unparse_node_with_spans(a, mod);
    }

    std::string unparse_node(Arena& a, NodeAny n) {
        Unparser u{a};
        run_any(u, n);
        return u.w.to_string(IndentStyle{}.text.c_str());
    }

    CodeOutput unparse_node_with_spans(Arena& a, NodeAny n) {
        Unparser u{a};
        run_any(u, n);
        return finish(u.w);
    }

    CodeWriter unparse_writer(Arena& a, NodeAny n) {
        Unparser u{a};
        run_any(u, n);
        return std::move(u.w);
    }

}  // namespace brgen::nast
