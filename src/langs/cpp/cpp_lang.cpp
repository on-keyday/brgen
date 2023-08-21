/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"
#include <core/writer/section.h>
#include "cpp_lang.h"
#include <core/common/error.h>

namespace brgen::cpp_lang {
    using writer::SectionPtr;
    void write_expr(const SectionPtr& w, ast::Expr* expr);
    void write_block(const SectionPtr& w, ast::node_list& elements, bool last_should_return);

    void write_binary(const SectionPtr& w, ast::Binary* b) {
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w->write("(");
        }
        write_expr(w, b->left.get());
        w->write(" ", *ast::bin_op_str(op), " ");
        write_expr(w, b->right.get());
        if (paren) {
            w->write(")");
        }
    }

    void write_block_scope(const SectionPtr& w, ast::IndentScope* block, bool last_should_be_return) {
        auto b = w->add_section("block", true).value();
        b->head().writeln("{");
        b->foot().writeln("}");
        write_block(b, block->elements, last_should_be_return);
    }

    void write_if_stmt(const SectionPtr& w, ast::If* if_, bool last_should_be_return) {
        auto if_stmt = w->add_section(".").value();
        auto cond = if_stmt->add_section("cond").value();
        cond->head().write("if(");
        cond->foot().write(") ");
        write_expr(cond, if_->cond.get());
        write_block_scope(if_stmt, if_->block.get(), last_should_be_return);
        if (if_->els) {
            w->write("else ");
            if (auto elif = ast::as<ast::If>(if_->els)) {
                write_if_stmt(w, elif, last_should_be_return);
            }
            else if (auto els = ast::as<ast::IndentScope>(if_->els)) {
                write_block_scope(w, els, last_should_be_return);
            }
        }
    }

    void write_expr(const SectionPtr& w, ast::Expr* expr) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            write_binary(w, b);
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
            write_expr(w, paren->expr.get());
            w->write(")");
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
            if (if_->expr_type->type == ast::NodeType::void_type) {
                write_if_stmt(w, if_, false);
            }
            else {
                auto lambda = w->add_section(".", true).value();
                lambda->head().writeln("[&]{");
                lambda->foot().write("}()");
                write_if_stmt(lambda, if_, true);
            }
        }
        else if (auto cond = ast::as<ast::Cond>(expr)) {
            w->write("(");
            write_expr(w, cond->cond.get());
            w->write(")?(");
            write_expr(w, cond->then.get());
            w->write("):(");
            write_expr(w, cond->els.get());
            w->write(")");
        }
        else if (auto unary = ast::as<ast::Unary>(expr)) {
            w->write(ast::unary_op[int(unary->op)], " ");
            write_expr(w, unary->target.get());
        }
    }

    void write_block(const SectionPtr& w, ast::node_list& elements, bool last_should_be_return) {
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto& element = *it;
            auto stmt = w->add_section(".").value();
            if (last_should_be_return && it == --elements.end()) {
                stmt->write("return ");
            }
            if (auto a = ast::as_Expr(element)) {
                write_expr(stmt, a);
                stmt->writeln(";");
            }
            else if (auto f = ast::as<ast::Field>(element)) {
                if (f->field_type->type != ast::NodeType::int_type) {
                    error(f->field_type->loc, "currently int type is only supported").report();
                }
                if (f->ident) {
                    stmt->writeln("int ", f->ident->ident, ";");
                    if (auto b = f->belong.lock()) {
                        auto path = "/global/struct/def/" + b->ident_path() + "/member/" + f->ident->ident;
                        auto found = w->lookup(path);
                        if (!found) {
                            auto d = w->add_section(path).value();
                            d->writeln("int ", f->ident->ident, ";");
                        }
                    }
                }
            }
            else if (auto n = ast::as<ast::Fmt>(element)) {
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
                        enc->writeln("int encode();");
                        dec->writeln("int decode();");
                    }
                    auto enc = fn_def->add_section("encode").value();
                    auto dec = fn_def->add_section("decode").value();
                    enc->head().write("int ", path, "::encode() ");
                    dec->head().write("int ", path, "::decode() ");
                    write_block_scope(enc, n->scope.get(), false);
                    write_block_scope(dec, n->scope.get(), false);
                }
            }
        }
    }

    result<SectionPtr> entry(Context& w, std::shared_ptr<ast::Program>& p) {
        try {
            auto root = writer::root();
            root->head().writeln("#include<cstdint>");
            root->head().writeln("#include<cstddef>");
            auto global = root->add_section("global").value();
            auto struct_ = global->add_section("struct").value();
            auto struct_dec = struct_->add_section("dec").value();
            struct_dec->writeln("struct Input {std::size_t bit_index = 0; const std::uint8_t buffer[1200];};");
            auto struct_def = struct_->add_section("def");
            auto fn = global->add_section("func").value();
            auto fn_dec = fn->add_section("dec").value();
            auto fn_def = fn->add_section("def").value();
            auto main_ = root->add_section("main", true).value();
            main_->head().writeln("int main() {");
            main_->foot().writeln("}");
            main_->writeln("Input input__{},*input=&input__;");
            write_block(main_, p->elements, false);
            return root;
        } catch (LocationError& e) {
            return unexpect(e);
        } catch (either::bad_expected_access<std::string>& s) {
            return unexpect(error({}, s.error()));
        }
    }

}  // namespace brgen::cpp_lang
