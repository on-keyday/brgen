/*license*/
#include "unparse.h"

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
            std::string out;
            int depth = 0;

            void nl() {
                out += "\n";
                for (int i = 0; i < depth; i++) {
                    out += "    ";
                }
            }

            std::string_view ident_text(Node<Ident> id) {
                auto d = id.ref(a);
                return d ? std::string_view(d->identifier) : std::string_view("$missing");
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
                        out += ", ";
                    }
                    first = false;
                    if (auto na = arg.as_any<NamedArgument>()) {
                        expr(na.ref(a)->name);
                        out += " = ";
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
                out += "<";
                type(t);
                out += ">";
            }

            void escape_into(std::string_view s) {
                for (char c : s) {
                    if (c == '\\' || c == '"') {
                        out += '\\';
                    }
                    out += c;
                }
            }

            void expr(Node<Expr> e) {
                if (!e) {
                    out += "/*missing*/";
                    return;
                }
                switch (e.type()) {
                    case NodeType::IntLiteral:
                        out += e.as<IntLiteral>().ref(a)->value;
                        return;
                    case NodeType::BoolLiteral:
                        out += e.as<BoolLiteral>().ref(a)->value ? "true" : "false";
                        return;
                    case NodeType::StrLiteral:
                        out += e.as<StrLiteral>().ref(a)->value;
                        return;
                    case NodeType::CharLiteral:
                        out += e.as<CharLiteral>().ref(a)->value;
                        return;
                    case NodeType::RegexLiteral:
                        out += e.as<RegexLiteral>().ref(a)->value;
                        return;
                    case NodeType::SpecialLiteral:
                        out += to_string(e.as<SpecialLiteral>().ref(a)->kind, true);
                        return;
                    case NodeType::Reference:
                        out += ident_text(e.as<Reference>().ref(a)->name);
                        return;
                    case NodeType::MemberAccess: {
                        auto d = e.as<MemberAccess>().ref(a);
                        expr(d->base);
                        out += ".";
                        out += ident_text(d->member);
                        return;
                    }
                    case NodeType::Index: {
                        auto d = e.as<Index>().ref(a);
                        expr(d->base);
                        out += "[";
                        expr(d->index);
                        out += "]";
                        return;
                    }
                    case NodeType::Call: {
                        auto d = e.as<Call>().ref(a);
                        expr(d->callee);
                        out += "(";
                        arguments(d->arguments);
                        out += ")";
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
                        out += "(";
                        arguments(d->arguments);
                        out += ")";
                        return;
                    }
                    case NodeType::TypeLiteral:
                        type_literal(e.as<TypeLiteral>());
                        return;
                    case NodeType::Paren: {
                        out += "(";
                        auto before = out.size();
                        expr(e.as<Paren>().ref(a)->expr);
                        // 中身が if 式などでブロックを開いていたら、閉じ括弧は
                        // 次の行に置かないと再 parse できない。
                        if (out.find('\n', before) != std::string::npos) {
                            nl();
                        }
                        out += ")";
                        return;
                    }
                    case NodeType::Unary: {
                        auto d = e.as<Unary>().ref(a);
                        out += spell(d->op);
                        expr(d->target);
                        return;
                    }
                    case NodeType::Binary: {
                        auto d = e.as<Binary>().ref(a);
                        expr(d->left);
                        if (d->op == BinaryOp::comma) {
                            out += ", ";
                        }
                        else {
                            out += " ";
                            out += spell(d->op);
                            out += " ";
                        }
                        expr(d->right);
                        return;
                    }
                    case NodeType::Range: {
                        auto d = e.as<Range>().ref(a);
                        auto before = out.size();
                        if (d->start) {
                            expr(d->start);
                        }
                        // start が match / if でブロックを開いていたら演算子は
                        // 次の行へ。parse は文末の改行を読み飛ばした後の範囲
                        // 演算子を式の続きとして拾う (tree_test.bgn の `..=`)。
                        if (out.find('\n', before) != std::string::npos) {
                            nl();
                        }
                        out += spell(d->op);
                        if (d->end) {
                            expr(d->end);
                        }
                        return;
                    }
                    case NodeType::Cond: {
                        auto d = e.as<Cond>().ref(a);
                        expr(d->cond);
                        out += " ? ";
                        expr(d->then);
                        out += " : ";
                        expr(d->els);
                        return;
                    }
                    case NodeType::Available:
                        out += "available(";
                        expr(e.as<Available>().ref(a)->target);
                        out += ")";
                        return;
                    case NodeType::Sizeof:
                        out += "sizeof(";
                        expr(e.as<Sizeof>().ref(a)->target);
                        out += ")";
                        return;
                    case NodeType::OrCond:
                        // base が元のカンマ連結。conds は同じ葉を共有している。
                        expr(e.as<OrCond>().ref(a)->base);
                        return;
                    case NodeType::Identity:
                        expr(e.as<Identity>().ref(a)->expr);
                        return;
                    case NodeType::Import:
                        out += "config.import(\"";
                        escape_into(e.as<Import>().ref(a)->path);
                        out += "\")";
                        return;
                    case NodeType::If:
                        if_statement(e.as<If>());
                        return;
                    case NodeType::Match:
                        match_statement(e.as<Match>());
                        return;
                    default:
                        out += "/*unprintable ";
                        out += to_string(e.type());
                        out += "*/";
                        return;
                }
            }

            // ---- 型 ------------------------------------------------------

            void type(Node<Type> t) {
                if (!t) {
                    out += "/*missing type*/";
                    return;
                }
                switch (t.type()) {
                    case NodeType::IntType: {
                        auto d = t.as<IntType>().ref(a);
                        out += d->is_signed ? "i" : "u";
                        if (d->endian == Endian::big) {
                            out += "b";
                        }
                        else if (d->endian == Endian::little) {
                            out += "l";
                        }
                        out += std::to_string(d->bit_size);
                        return;
                    }
                    case NodeType::FloatType: {
                        auto d = t.as<FloatType>().ref(a);
                        out += "f";
                        if (d->endian == Endian::big) {
                            out += "b";
                        }
                        else if (d->endian == Endian::little) {
                            out += "l";
                        }
                        out += std::to_string(d->bit_size);
                        return;
                    }
                    case NodeType::BoolType:
                        out += "bool";
                        return;
                    case NodeType::VoidType:
                        out += "void";
                        return;
                    case NodeType::IdentType:
                        out += ident_text(t.as<IdentType>().ref(a)->ident);
                        return;
                    case NodeType::ImportedType:
                        expr(t.as<ImportedType>().ref(a)->import_ref);
                        return;
                    case NodeType::ArrayType: {
                        auto d = t.as<ArrayType>().ref(a);
                        out += "[";
                        if (d->length) {
                            expr(d->length);
                        }
                        out += "]";
                        type(d->element_type);
                        return;
                    }
                    case NodeType::StrLiteralType: {
                        auto lit = t.as<StrLiteralType>().ref(a)->base.ref(a);
                        out += lit ? lit->value : "/*missing literal*/";
                        return;
                    }
                    case NodeType::RegexLiteralType: {
                        auto lit = t.as<RegexLiteralType>().ref(a)->base.ref(a);
                        out += lit ? lit->value : "/*missing literal*/";
                        return;
                    }
                    case NodeType::FunctionType: {
                        auto d = t.as<FunctionType>().ref(a);
                        out += "fn (";
                        bool first = true;
                        for (auto& p : d->parameters) {
                            if (!first) {
                                out += ", ";
                            }
                            first = false;
                            out += ":";
                            type(p);
                        }
                        out += ")";
                        if (d->return_type) {
                            out += " -> ";
                            type(d->return_type);
                        }
                        return;
                    }
                    case NodeType::GenericType: {
                        auto d = t.as<GenericType>().ref(a);
                        type(d->base_type);
                        out += "[";
                        bool first = true;
                        for (auto& arg : d->type_arguments) {
                            if (!first) {
                                out += ", ";
                            }
                            first = false;
                            type(arg);
                        }
                        out += "]";
                        return;
                    }
                    case NodeType::InlineStructType: {
                        auto fmt = t.as<InlineStructType>().ref(a)->inlined_format;
                        out += "format";
                        if (auto name = fmt.ref(a)->name) {
                            out += " ";
                            out += ident_text(name);
                        }
                        out += ":";
                        body(fmt.ref(a)->body);
                        return;
                    }
                    default:
                        out += "/*unprintable type ";
                        out += to_string(t.type());
                        out += "*/";
                        return;
                }
            }

            // ---- 文 ------------------------------------------------------

            void body(Node<Body> b) {
                depth++;
                auto d = b.ref(a);
                if (d) {
                    for (auto& s : d->statements) {
                        nl();
                        statement(s);
                    }
                }
                depth--;
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
                            out += "if ";
                        }
                        else {
                            nl();
                            out += "elif ";
                        }
                        expr(cs.ref(a)->condition);
                        out += ":";
                    }
                    else {
                        nl();
                        out += "else:";
                    }
                    first = false;
                    body(block.ref(a)->body);
                }
            }

            void match_statement(Node<Match> n) {
                auto d = n.ref(a);
                out += "match";
                if (d->condition) {
                    out += " ";
                    expr(d->condition);
                }
                out += ":";
                depth++;
                for (auto& block : d->blocks) {
                    nl();
                    auto cs = block.as<ConditionalStatement>().ref(a);
                    expr(cs->condition);
                    auto b = cs->body.ref(a);
                    // `=> 文` と 1 文だけのブロックは同じ木。1 行に置ける文なら
                    // `=>` で書く。
                    if (b && b->statements.size() == 1 && fits_on_a_line(b->statements[0])) {
                        out += " => ";
                        statement(b->statements[0]);
                    }
                    else {
                        out += ":";
                        body(cs->body);
                    }
                }
                depth--;
            }

            void loop_statement(Node<Loop> n) {
                auto d = n.ref(a);
                out += "for";
                if (!d->init && !d->condition && !d->step) {
                    out += ":";
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
                    out += " ";
                    if (cond_only) {
                        expr(d->condition);
                    }
                    else {
                        if (d->init) {
                            statement(d->init);
                        }
                        out += ";";
                        if (d->condition) {
                            out += " ";
                            expr(d->condition);
                        }
                        if (d->step) {
                            out += "; ";
                            statement(d->step);
                        }
                    }
                    out += ":";
                }
                body(d->body);
            }

            void enum_statement(Node<Enum> n) {
                auto d = n.ref(a);
                out += "enum ";
                out += ident_text(d->name);
                out += ":";
                depth++;
                if (d->base_type) {
                    nl();
                    out += ":";
                    type(d->base_type);
                }
                for (auto& m : d->members) {
                    nl();
                    auto md = m.ref(a);
                    out += ident_text(md->name);
                    // 値は書かれなかったものも parse が合成する。書かれたもの
                    // (raw_expr) だけを書き、合成分は再 parse に作り直させる。
                    if (md->raw_expr) {
                        out += " = ";
                        expr(md->raw_expr);
                    }
                }
                depth--;
            }

            void fn_statement(Node<Function> n) {
                auto d = n.ref(a);
                out += "fn ";
                out += ident_text(d->name);
                out += "(";
                bool first = true;
                for (auto& p : d->parameters) {
                    if (!first) {
                        out += ", ";
                    }
                    first = false;
                    auto pd = p.ref(a);
                    if (pd->name) {
                        out += ident_text(pd->name);
                        out += " ";
                    }
                    out += ":";
                    type(pd->type);
                }
                out += ")";
                // 書かれなかった戻り値は parse が VoidType を合成する。合成分は
                // is_explicit が立たないのでそこで見分ける。
                if (d->return_type &&
                    !(d->return_type.as_any<VoidType>() && !d->return_type.ref(a)->is_explicit)) {
                    out += " -> ";
                    type(d->return_type);
                }
                out += ":";
                body(d->body);
            }

            void field_statement(Node<NamedTypeStatement> n, Node<Arguments> args) {
                auto d = n.ref(a);
                if (d->name) {
                    out += ident_text(d->name);
                    out += " ";
                }
                out += ":";
                type(d->type);
                if (args) {
                    out += "(";
                    arguments(args);
                    out += ")";
                }
            }

            void statement(Node<Statement> s) {
                if (!s) {
                    out += "/*missing statement*/";
                    return;
                }
                switch (s.type()) {
                    case NodeType::Format: {
                        auto d = s.as<Format>().ref(a);
                        out += "format ";
                        out += ident_text(d->name);
                        out += ":";
                        body(d->body);
                        return;
                    }
                    case NodeType::GenericFormat: {
                        auto d = s.as<GenericFormat>().ref(a);
                        out += "format ";
                        out += ident_text(d->name);
                        out += "[";
                        bool first = true;
                        for (auto& tp : d->type_parameters) {
                            if (!first) {
                                out += ", ";
                            }
                            first = false;
                            out += ident_text(tp.ref(a)->name);
                        }
                        out += "]:";
                        body(d->body);
                        return;
                    }
                    case NodeType::State: {
                        auto d = s.as<State>().ref(a);
                        out += "state ";
                        out += ident_text(d->name);
                        out += ":";
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
                        out += "for ";
                        out += ident_text(bind->name);
                        out += " in ";
                        expr(bind->value);
                        out += ":";
                        body(d->body);
                        return;
                    }
                    case NodeType::Return: {
                        auto d = s.as<Return>().ref(a);
                        out += "return";
                        if (d->expr) {
                            out += " ";
                            expr(d->expr);
                        }
                        return;
                    }
                    case NodeType::Break:
                        out += "break";
                        return;
                    case NodeType::Continue:
                        out += "continue";
                        return;
                    case NodeType::Assert:
                        // 真偽演算子の文は parse が Assert に包む。式だけ書けば
                        // 再 parse が包み直す。
                        expr(s.as<Assert>().ref(a)->expr);
                        return;
                    case NodeType::Assign: {
                        auto d = s.as<Assign>().ref(a);
                        expr(d->assignee);
                        out += " ";
                        out += spell(d->op);
                        out += " ";
                        expr(d->value);
                        return;
                    }
                    case NodeType::VariableDefinition: {
                        auto d = s.as<VariableDefinition>().ref(a);
                        out += ident_text(d->name);
                        out += " ";
                        out += spell(d->op);
                        out += " ";
                        expr(d->value);
                        return;
                    }
                    case NodeType::Metadata: {
                        auto d = s.as<Metadata>().ref(a);
                        out += d->name;
                        auto args = d->arguments.ref(a);
                        // 代入形と 1 引数の呼び出し形は同じ木。代入形で書く。
                        if (args && args->arguments.size() == 1) {
                            out += " = ";
                            expr(args->arguments[0].ref(a)->value);
                        }
                        else {
                            out += "(";
                            arguments(d->arguments);
                            out += ")";
                        }
                        return;
                    }
                    case NodeType::SpecifyOrder: {
                        auto d = s.as<SpecifyOrder>().ref(a);
                        out += d->name;
                        out += " = ";
                        expr(d->order);
                        return;
                    }
                    case NodeType::ExplicitError: {
                        // extra_arguments は message も含む元の引数列そのもの。
                        out += "error(";
                        arguments(s.as<ExplicitError>().ref(a)->extra_arguments);
                        out += ")";
                        return;
                    }
                    default:
                        if (auto e = s.as_any<Expr>()) {
                            expr(e);
                            return;
                        }
                        out += "/*unprintable statement ";
                        out += to_string(s.type());
                        out += "*/";
                        return;
                }
            }
        };

    }  // namespace

    std::string unparse(Arena& a, Node<Module> mod) {
        Unparser u{a};
        auto d = mod.ref(a);
        if (d) {
            bool first = true;
            for (auto& s : d->statements) {
                if (!first) {
                    u.nl();
                }
                first = false;
                u.statement(s);
            }
        }
        u.out += "\n";
        return u.out;
    }

}  // namespace brgen::nast
