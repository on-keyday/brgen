/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/node/translated.h"
#include <core/writer/section.h>
#include "go_lang.h"
#include <core/common/error.h>

namespace brgen::go_lang {
    using writer::SectionPtr;
    void write_expr(Context& c, const SectionPtr& w, ast::Expr* expr);
    void write_block(Context& c, const SectionPtr& w, ast::node_list& elements);

    std::string get_type_text(Context& c, const SectionPtr& w, std::shared_ptr<ast::Type>& ty) {
        if (ty->type != ast::NodeType::int_type) {
            error(ty->loc, "currently int type is only supported").report();
        }
        auto t = ast::as<ast::IntType>(ty);
        return concat("uint", nums(ast::aligned_bit(t->bit_size)));
    }

    void write_binary(Context& c, const SectionPtr& w, ast::Binary* b) {
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w->write("(");
        }
        write_expr(c, w, b->left.get());
        if (auto id = ast::as<ast::Ident>(b->left);
            id &&
            id->usage != ast::IdentUsage::reference) {
            w->write(" :", *ast::bin_op_str(op), " ");
        }
        else {
            w->write(" ", *ast::bin_op_str(op), " ");
        }
        write_expr(c, w, b->right.get());
        if (paren) {
            w->write(")");
        }
    }

    void write_block_scope(Context& c, const SectionPtr& w, ast::IndentScope* block, bool need_ln) {
        auto b = w->add_section("block", true).value();
        b->head().writeln("{");
        if (need_ln) {
            b->foot().writeln("}");
        }
        else {
            b->foot().write("}");
        }
        write_block(c, b, block->elements);
    }

    void write_if_stmt(Context& c, const SectionPtr& w, ast::If* if_) {
        auto if_stmt = w->add_section(".").value();
        auto cond = if_stmt->add_section("cond").value();
        cond->head().write("if(");
        cond->foot().write(") ");
        write_expr(c, cond, if_->cond.get());
        write_block_scope(c, if_stmt, if_->block.get(), false);
        if (if_->els) {
            w->write("else ");
            if (auto elif = ast::as<ast::If>(if_->els)) {
                write_if_stmt(c, w, elif);
            }
            else if (auto els = ast::as<ast::IndentScope>(if_->els)) {
                write_block_scope(c, w, els, true);
            }
        }
        else {
            w->writeln("");
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
                w->write(ident->ident);
            }
            else {
                w->write(ident->ident);
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
                write_if_stmt(c, w, if_);
            }
            else {
                auto lambda = w->add_section(".", true).value();
                lambda->head().writeln("func() ", get_type_text(c, w, if_->expr_type), " {");
                lambda->foot().write("}()");
                write_if_stmt(c, lambda, if_);
            }
        }
        else if (auto cond = ast::as<ast::Cond>(expr)) {
            w->writeln("func() ", get_type_text(c, w, cond->expr_type), " {");
            w->write("if ");
            write_expr(c, w, cond->cond.get());
            w->writeln("{");
            auto s = w->add_section(".", true).value();
            s->write("return ");
            write_expr(c, w, cond->then.get());
            w->writeln("} else {");
            w->write("return ");
            write_expr(c, w, cond->els.get());
            w->writeln("}");
            w->writeln("}()");
        }
        else if (auto unary = ast::as<ast::Unary>(expr)) {
            w->write(ast::unary_op[int(unary->op)], " ");
            write_expr(c, w, unary->target.get());
        }
        else {
            error(expr->loc, "unsupported operation").report();
        }
    }

    void write_bit_io_code(Context& c, const SectionPtr& w, writer::BitIOCodeGenerator& io) {
        auto write_code = [&](writer::Writer& eb) {
            if (!io.is_encode) {  // decode
                eb.writeln(io.target, " = 0;");
            }
            decltype(eb.indent_scope_ex()) dec_scope;
            auto add_field_debug = [&](auto&& var) {
                if (c.config.insert_bit_pos_debug_code) {
                    eb.writeln(R"a(fmt.Printf("%s: %d",")a", var, R"a(",)a", var, ")");
                }
            };

            eb.writeln(io.define_begin_bits("begin_bits"));
            add_field_debug(io.non_aligned_bit_size_v);

            if (io.bit_size_v <= 7) {
                // if remaining bits are enough to make value
                eb.writeln("if ", io.enough_bits_for_bit_size_value(), " {");
                eb.indent_writeln(io.code_less_than_8_bit(), ";");
                eb.write("}");
                // in this case, bytes are not available and begin_bits == 0 so shortcut
                if (io.bit_size_v == 1) {
                    eb.writeln("else {");
                    eb.indent_writeln(io.code_end_1_bit());
                    eb.writeln("}");
                    return;
                }
                eb.write("else {");
                dec_scope = eb.indent_scope_ex();
            }
            eb.writeln(io.define_remain_bits("remain_bits"));
            eb.writeln(io.define_bytes("bytes"));
            eb.writeln(io.define_end_bits("end_bits"));

            add_field_debug(io.bytes_v);
            add_field_debug(io.tail_bits_v);

            eb.writeln("if ", io.non_aligned_bits_exists(), " {");
            eb.indent_writeln(io.code_begin_bits(), ";");
            eb.writeln("}");

            if (io.bit_size_v >= 8) {  // less than 8 bit type has no bytes so skip
                eb.writeln(io.define_bytes_base("base"));
                /*
                eb.writeln("[[assume(", io.bytes_v, " == ", nums(io.bit_size_v / 8),
                           " || ", io.bytes_v, " == ", nums(io.bit_size_v / 8 - 1), ")]];");
                */
                eb.writeln("for i := 0; i < ", io.bytes_v, "; i++ {");
                eb.indent_writeln(io.code_bytes("i"), ";");
                eb.writeln("}");
            }

            eb.writeln("if ", io.tail_bits_exists(), " {");
            eb.indent_writeln(io.code_end_bits(), ";");
            eb.writeln("}");
            if (dec_scope) {
                dec_scope->execute();
                eb.writeln("}");
            }
        };
        if (io.target.size()) {
            auto s = w->add_section(".", true).value();
            auto& eb = s->head();
            eb.writeln("{");
            {
                auto sp = eb.indent_scope();
                write_code(eb);
                if (c.config.insert_bit_pos_debug_code) {
                    eb.writeln(R"(fmt.Println())");
                }
            }
            eb.writeln("}");
        }
        w->writeln(io.add_offset(), ";");
    }

    void write_encode(Context& c, const SectionPtr& w, std::shared_ptr<ast::Type>& ty, std::string_view target) {
        writer::BitIOCodeGenerator io;
        io.io_object = "output";
        io.accessor = ".";
        io.buffer = "buffer";
        io.index = "bitIndex";
        io.byte_type = "uint8";
        auto base_ty = get_type_text(c, w, ty);
        io.base_type = base_ty;
        io.bit_size_v = ast::as<ast::IntType>(ty)->bit_size;
        io.target = target;
        io.is_encode = true;
        io.define_symbol = ":=";

        write_bit_io_code(c, w, io);
    }

    void write_decode(Context& c, const SectionPtr& w, std::shared_ptr<ast::Type>& ty, std::string_view target) {
        writer::BitIOCodeGenerator io;
        io.io_object = "input";
        io.accessor = ".";
        io.buffer = "buffer";
        io.index = "bitIndex";
        io.byte_type = "uint8";
        auto base_ty = get_type_text(c, w, ty);
        io.base_type = base_ty;
        io.bit_size_v = ast::as<ast::IntType>(ty)->bit_size;
        io.target = target;
        io.is_encode = false;
        io.define_symbol = ":=";

        write_bit_io_code(c, w, io);
    }

    void write_block(Context& c, const SectionPtr& w, ast::node_list& elements) {
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto& element = *it;
            auto stmt = w->add_section(".").value();
            if (auto a = ast::as<ast::ImplicitReturn>(element)) {
                stmt->write("return ");
                write_expr(c, stmt, a->expr.get());
                stmt->writeln(";");
            }
            else if (auto a = ast::as<ast::Expr>(element)) {
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
                            d->writeln(f->ident->ident, " ", get_type_text(c, w, f->field_type));
                        }
                    }
                    else {
                        stmt->writeln("var ", f->ident->ident, " ", get_type_text(c, w, f->field_type));
                    }
                }
                if (c.mode == writer::WriteMode::encode) {
                    write_encode(c, w, f->field_type, f->ident ? "self." + f->ident->ident : "");
                }
                else if (c.mode == writer::WriteMode::decode) {
                    write_decode(c, w, f->field_type, f->ident ? "self." + f->ident->ident : "");
                }
            }
            else if (auto n = ast::as<ast::Format>(element)) {
                if (!c.def_done) {
                    auto path = n->ident_path();
                    // add structs
                    auto dec = w->add_section("/global/struct/dec/" + path, true).value();
                    auto def = w->add_section("/global/struct/def/" + path, true).value();
                    {
                        def->head().writeln("type ", n->ident, " struct {");
                        def->foot().writeln("};");
                    }
                    // add functions
                    {
                        auto fn_dec = w->add_section("/global/struct/def/" + path + "/member").value();
                        auto fn_def = w->add_section("/global/func/def/" + path).value();

                        auto enc = fn_def->add_section("encode").value();
                        auto dec = fn_def->add_section("decode").value();
                        enc->head().write("func (self *", path, ") encode(output *Output) ");
                        dec->head().write("func (self *", path, ") decode(input *Input) ");
                        {
                            auto m = c.set_write_mode(writer::WriteMode::encode);
                            write_block_scope(c, enc, n->scope.get(), true);
                        }
                        {
                            auto m = c.set_write_mode(writer::WriteMode::decode);
                            write_block_scope(c, dec, n->scope.get(), true);
                        }
                    }
                }
            }
        }
    }

    result<SectionPtr> entry(Context& c, std::shared_ptr<ast::Program>& p) {
        try {
            auto root = writer::root();
            root->writeln("// Code generated by brgen (https://github.com/on-keyday/brgen). DO NOT EDIT.");
            root->writeln("package main");
            auto hdr = root->add_section("import", true).value();
            hdr->head().writeln("import (");
            hdr->foot().writeln(")");
            hdr->writeln(R"(_ "encoding/binary")");
            if (c.config.insert_bit_pos_debug_code) {
                hdr->writeln(R"("fmt")");
            }
            auto global = root->add_section("global").value();
            auto struct_ = global->add_section("struct").value();
            auto struct_dec = struct_->add_section("dec").value();
            struct_dec->writeln("type Input struct { bitIndex int; buffer []byte }");
            struct_dec->writeln("type Output struct { bitIndex int; buffer []byte };");
            auto struct_def = struct_->add_section("def");
            auto fn = global->add_section("func").value();
            auto fn_dec = fn->add_section("dec").value();
            auto fn_def = fn->add_section("def").value();
            auto main_ = root->add_section("main").value();
            {
                auto enc = main_->add_section("encode").value();
                enc->head().writeln("func main_encode(output *Output) {");
                enc->foot().writeln("}");
                write_block(c, enc, p->elements);
            }
            c.def_done = true;
            {
                auto dec = main_->add_section("decode").value();
                dec->head().writeln("func main_decode(input *Input) {");
                dec->foot().writeln("}");
                write_block(c, dec, p->elements);
            }
            if (c.config.test_main) {
                main_->writeln("func main(){}");
            }
            return root;
        } catch (LocationError& e) {
            return unexpect(e);
        } catch (either::bad_expected_access<std::string>& s) {
            return unexpect(error({}, s.error()));
        }
    }

}  // namespace brgen::go_lang
