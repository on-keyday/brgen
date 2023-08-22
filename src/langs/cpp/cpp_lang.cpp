/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"
#include <core/writer/section.h>
#include "cpp_lang.h"
#include <core/common/error.h>

namespace brgen::cpp_lang {
    using writer::SectionPtr;
    void write_expr(Context& c, const SectionPtr& w, ast::Expr* expr);
    void write_block(Context& c, const SectionPtr& w, ast::node_list& elements);

    void write_binary(Context& c, const SectionPtr& w, ast::Binary* b) {
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w->write("(");
        }
        write_expr(c, w, b->left.get());
        w->write(" ", *ast::bin_op_str(op), " ");
        write_expr(c, w, b->right.get());
        if (paren) {
            w->write(")");
        }
    }

    void write_block_scope(Context& c, const SectionPtr& w, ast::IndentScope* block) {
        auto b = w->add_section("block", true).value();
        b->head().writeln("{");
        b->foot().writeln("}");
        write_block(c, b, block->elements);
    }

    void write_if_stmt(Context& c, const SectionPtr& w, ast::If* if_) {
        auto if_stmt = w->add_section(".").value();
        auto cond = if_stmt->add_section("cond").value();
        cond->head().write("if(");
        cond->foot().write(") ");
        write_expr(c, cond, if_->cond.get());
        write_block_scope(c, if_stmt, if_->block.get());
        if (if_->els) {
            w->write("else ");
            if (auto elif = ast::as<ast::If>(if_->els)) {
                write_if_stmt(c, w, elif);
            }
            else if (auto els = ast::as<ast::IndentScope>(if_->els)) {
                write_block_scope(c, w, els);
            }
        }
    }

    void write_expr(Context& c, const SectionPtr& w, ast::Expr* expr) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            write_binary(c, w, b);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (ident->usage == ast::IdentUsage::reference) {
                w->write(ident->ident);
            }
            else if (ident->usage == ast::IdentUsage::define_const) {
                w->write("const int ", ident->ident);
            }
            else {
                w->write("int ", ident->ident);
            }
        }
        else if (auto num = ast::as<ast::IntLiteral>(expr)) {
            w->write("0x", nums(*num->parse_as<std::uint64_t>(), 16));
        }
        else if (auto call = ast::as<ast::Call>(expr)) {
        }
        else if (auto paren = ast::as<ast::Paren>(expr)) {
            w->write("(");
            write_expr(c, w, paren->expr.get());
            w->write(")");
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
            if (if_->expr_type->type == ast::NodeType::void_type) {
                auto old = c.set_last_should_be_return(false);
                write_if_stmt(c, w, if_);
            }
            else {
                auto lambda = w->add_section(".", true).value();
                lambda->head().writeln("[&]{");
                lambda->foot().write("}()");
                auto ol = c.set_last_should_be_return(true);
                write_if_stmt(c, lambda, if_);
            }
        }
        else if (auto cond = ast::as<ast::Cond>(expr)) {
            w->write("(");
            write_expr(c, w, cond->cond.get());
            w->write(")?(");
            write_expr(c, w, cond->then.get());
            w->write("):(");
            write_expr(c, w, cond->els.get());
            w->write(")");
        }
        else if (auto unary = ast::as<ast::Unary>(expr)) {
            w->write(ast::unary_op[int(unary->op)], " ");
            write_expr(c, w, unary->target.get());
        }
    }

    void append_bits_to_target(writer::Writer& eb, std::string_view target, std::string_view expression, bool assign = false) {
        if (assign) {
            eb.writeln(target, "=", expression, ";");
        }
        else {
            eb.writeln(target, "|=", expression, ";");
        }
    }

    void write_encode_section(writer::Writer& eb, std::string_view target, std::string_view expression, bool assign = false) {
        eb.writeln("{");
        {
            auto sc = eb.indent_scope();
            append_bits_to_target(eb, target, expression, assign);
        }
        eb.writeln("}");
    }

    void write_encode(Context& c, const SectionPtr& w, std::shared_ptr<ast::Type>& ty, std::string_view target) {
        if (ty->type != ast::NodeType::int_type) {
            error(ty->loc, "currently int type is only supported").report();
        }
        auto t = ast::as<ast::IntegerType>(ty);
        auto base_ty = concat("std::uint", nums(ast::aligned_bit(t->bit_size)), "_t");
        auto s = w->add_section(".", true).value();
        auto& eb = s->head();
        auto write_encoder = [&] {
            eb.writeln("std::uint8_t begin_bits = (8 - (output->bit_index & 0x7)) & 0x7;");
            auto b = t->bit_size >= 7
                         ? concat("(", nums(t->bit_size), " - begin_bits", ")")
                         : concat("(", nums(t->bit_size), " < begin_bits ? 0 :", nums(t->bit_size), "- begin_bits)");
            eb.writeln("size_t bytes = ", b, " >> 3;");
            eb.writeln("std::uint8_t end_bits = ", b, " & 0x7;");
            eb.writeln("[[assume(begin_bits < 8 && end_bits < 8)]];");

            eb.write("if(begin_bits!=0) ");
            write_encode_section(eb, "output->buffer[output->bit_index >> 3]",
                                 concat("std::uint8_t",
                                        "(",
                                        base_ty,
                                        "(", target, ")",
                                        ">>",
                                        "(",
                                        nums(t->bit_size),
                                        " - begin_bits",
                                        ")"
                                        ")"));

            if (t->bit_size >= 8) {
                eb.writeln("auto base = (output->bit_index + begin_bits) >> 3;");
                eb.writeln("[[assume(bytes == ", nums(t->bit_size / 8), " || bytes == ", nums(t->bit_size / 8 - 1), ")]];");
                eb.write("for(auto i = 0; i < bytes; i++) ");
                write_encode_section(eb, "output->buffer[base + i]",
                                     concat("std::uint8_t(",
                                            base_ty, "(", target, ")",
                                            ">>"
                                            " (",
                                            nums(t->bit_size),
                                            " - ((i + 1) << 3) - begin_bits"
                                            ")",
                                            ")"),
                                     true);
            }

            eb.write("if(end_bits!=0) ");
            write_encode_section(eb, "output->buffer[(output->bit_index + begin_bits + (bytes << 3)) >> 3]",
                                 concat("std::uint8_t", "(",
                                        base_ty, "(", target, ")",
                                        "<<",
                                        "(",
                                        "8 - end_bits",
                                        ")",
                                        ")"),
                                 true);
        };
        if (target.size()) {
            eb.writeln("{");
            {
                auto sp = eb.indent_scope();
                write_encoder();
            }
            eb.writeln("}");
        }
        w->writeln("output->bit_index +=", nums(t->bit_size), ";");
    }

    void write_decode(Context& c, const SectionPtr& w, std::shared_ptr<ast::Type>& ty, std::string_view target) {
        if (ty->type != ast::NodeType::int_type) {
            error(ty->loc, "currently int type is only supported").report();
        }
        auto t = ast::as<ast::IntegerType>(ty);
        auto base_ty = concat("std::uint", nums(ast::aligned_bit(t->bit_size)), "_t");
        auto s = w->add_section(".", true).value();
        auto& eb = s->head();
        auto write_decoder = [&] {
            eb.writeln(target, " = 0;");
            eb.writeln("std::uint8_t begin_bits = (8 - (input->bit_index & 0x7)) & 0x7;");
            auto b = t->bit_size >= 7
                         ? concat("(", nums(t->bit_size), " - begin_bits", ")")
                         : concat("(", nums(t->bit_size), " < begin_bits ? 0 :", nums(t->bit_size), "- begin_bits)");
            eb.writeln("size_t bytes = ", b, " >> 3;");
            eb.writeln("std::uint8_t end_bits = ", b, " & 0x7;");
            eb.writeln("[[assume(begin_bits < 8 && end_bits < 8)]];");

            eb.write("if(begin_bits!=0) ");
            write_encode_section(eb, target,
                                 concat(base_ty, "(", "input->buffer[input->bit_index >> 3]",
                                        "&",
                                        "(",
                                        "std::uint8_t(0xff) >> (input->bit_index & 0x7)",
                                        ")",
                                        ")",
                                        "<<"
                                        "(",
                                        nums(t->bit_size),
                                        " - begin_bits",
                                        ")"));

            if (t->bit_size >= 8) {
                eb.writeln("auto base = (input->bit_index + begin_bits) >> 3;");
                eb.writeln("[[assume(bytes == ", nums(t->bit_size / 8), " || bytes == ", nums(t->bit_size / 8 - 1), ")]];");
                eb.write("for(auto i = 0; i < bytes; i++) ");
                write_encode_section(eb, target,
                                     concat(base_ty,
                                            "(input->buffer[base + i])"
                                            "<<"
                                            " (",
                                            nums(t->bit_size),
                                            " - ((i + 1) << 3) - begin_bits"
                                            ")"));
            }

            eb.write("if(end_bits!=0) ");
            write_encode_section(eb, target,
                                 concat(base_ty, "(", "input->buffer[(input->bit_index + begin_bits + (bytes << 3)) >> 3]",
                                        ">>",
                                        "(",
                                        "8 - end_bits",
                                        ")",
                                        ")"));
        };
        if (target.size()) {
            eb.writeln("{");
            {
                auto sp = eb.indent_scope();
                write_decoder();
            }
            eb.writeln("}");
        }
        w->writeln("input->bit_index +=", nums(t->bit_size), ";");
    }

    void write_block(Context& c, const SectionPtr& w, ast::node_list& elements) {
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto& element = *it;
            auto stmt = w->add_section(".").value();
            if (c.last_should_be_return && it == --elements.end()) {
                stmt->write("return ");
            }
            if (auto a = ast::as_Expr(element)) {
                write_expr(c, stmt, a);
                stmt->writeln(";");
            }
            else if (auto f = ast::as<ast::Field>(element)) {
                if (f->ident) {
                    if (auto b = f->belong.lock()) {
                        auto path = "/global/struct/def/" + b->ident_path() + "/member/" + f->ident->ident;
                        auto found = w->lookup(path);
                        if (!found) {
                            auto d = w->add_section(path).value();
                            d->writeln("std::uint64_t ", f->ident->ident, ";");
                        }
                    }
                    else {
                        stmt->writeln("std::uint64_t ", f->ident->ident, ";");
                    }
                }
                if (c.mode == WriteMode::encode) {
                    write_encode(c, w, f->field_type, f->ident ? f->ident->ident : "");
                }
                else if (c.mode == WriteMode::decode) {
                    write_decode(c, w, f->field_type, f->ident ? f->ident->ident : "");
                }
            }
            else if (auto n = ast::as<ast::Fmt>(element)) {
                if (!c.def_done) {
                    auto path = n->ident_path();
                    // add structs
                    auto dec = w->add_section("/global/struct/dec/" + path, true).value();
                    auto def = w->add_section("/global/struct/def/" + path, true).value();
                    {
                        def->head().writeln("struct ", n->ident, " {");
                        def->foot().writeln("};");
                    }
                    // add functions
                    {
                        auto fn_dec = w->add_section("/global/struct/def/" + path + "/member").value();
                        auto fn_def = w->add_section("/global/func/def/" + path).value();
                        {
                            auto enc = fn_dec->add_section("encode").value();
                            auto dec = fn_dec->add_section("decode").value();
                            enc->writeln("void encode(Output*) const;");
                            dec->writeln("void decode(Input*);");
                        }
                        auto enc = fn_def->add_section("encode").value();
                        auto dec = fn_def->add_section("decode").value();
                        enc->head().write("void ", path, "::encode(Output* output) const ");
                        dec->head().write("void ", path, "::decode(Input* input) ");
                        auto old = c.set_last_should_be_return(false);
                        {
                            auto m = c.set_write_mode(WriteMode::encode);
                            write_block_scope(c, enc, n->scope.get());
                        }
                        {
                            auto m = c.set_write_mode(WriteMode::decode);
                            write_block_scope(c, dec, n->scope.get());
                        }
                    }
                }
            }
        }
    }

    result<SectionPtr> entry(Context& c, std::shared_ptr<ast::Program>& p) {
        try {
            auto root = writer::root();
            root->head().writeln("#include<cstdint>");
            root->head().writeln("#include<cstddef>");
            auto global = root->add_section("global").value();
            auto struct_ = global->add_section("struct").value();
            auto struct_dec = struct_->add_section("dec").value();
            struct_dec->writeln("struct Input {std::size_t bit_index = 0; const std::uint8_t* buffer=nullptr;};");
            struct_dec->writeln("struct Output { std::size_t bit_index = 0; std::uint8_t* buffer=nullptr; };");
            auto struct_def = struct_->add_section("def");
            auto fn = global->add_section("func").value();
            auto fn_dec = fn->add_section("dec").value();
            auto fn_def = fn->add_section("def").value();
            auto main_ = root->add_section("main").value();
            auto old = c.set_last_should_be_return(false);
            {
                auto enc = main_->add_section("encode").value();
                enc->head().writeln("void main_encode(Output* output) {");
                enc->foot().writeln("}");
                write_block(c, enc, p->elements);
            }
            c.def_done = true;
            {
                auto dec = main_->add_section("decode").value();
                dec->head().writeln("void main_decode(Input* input) {");
                dec->foot().writeln("}");
                write_block(c, dec, p->elements);
            }
            main_->writeln("int main(){}");
            return root;
        } catch (LocationError& e) {
            return unexpect(e);
        } catch (either::bad_expected_access<std::string>& s) {
            return unexpect(error({}, s.error()));
        }
    }

}  // namespace brgen::cpp_lang
