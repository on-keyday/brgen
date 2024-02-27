/*license*/
#include "vm.h"
#include <core/ast/ast.h>
#include <core/ast/traverse.h>
#include <core/ast/tool/stringer.h>
#include "compile.h"
#include <writer/writer.h>
namespace brgen::vm {

    struct FieldInfo {
        std::weak_ptr<ast::Format> fmt;
        size_t offset = 0;
    };

    struct FormatInfo {
        size_t current_offset = 0;
        size_t encode_entry = 0;
        size_t decode_entry = 0;
    };

    struct StateVariable {
        size_t offset = 0;
    };

    struct Compiler {
        Code code;

        std::map<std::shared_ptr<ast::Format>, FormatInfo> format_info;
        std::map<std::shared_ptr<ast::Field>, FieldInfo> field_info;
        std::map<std::shared_ptr<ast::Field>, StateVariable> state_variable_info;

        bool encode = false;
        size_t state_variable_offset = 0;

        size_t op(Op op, std::uint64_t arg = 0) {
            auto pc = code.instructions.size();
            code.instructions.push_back(Instruction{op, arg});
            return pc;
        }

        size_t pc() const {
            return code.instructions.size();
        }

        size_t add_static(const Value& value) {
            auto index = code.static_data.size();
            code.static_data.push_back(value);
            return index;
        }

        void rewrite_arg(size_t index, std::uint64_t arg) {
            code.instructions[index].arg(arg);
        }

        auto function_prologue(const std::string& func_name) {
            auto indexOfInstruction = op(Op::NEXT_FUNC);
            auto indexOfFuncName = add_static(Value{func_name});
            op(Op::FUNC_NAME, indexOfFuncName);
            return futils::helper::defer([&, indexOfInstruction] {
                op(Op::FUNC_END, indexOfInstruction);
                rewrite_arg(indexOfInstruction, pc());
            });
        }

        void compile_expr(const std::shared_ptr<ast::Expr>& expr) {
            if (auto i_lit = ast::as<ast::IntLiteral>(expr)) {
                op(Op::LOAD_IMMEDIATE, *i_lit->parse_as<std::uint64_t>());
                return;
            }
            if (auto str_lit = ast::as<ast::StrLiteral>(expr)) {
                auto index = add_static(Value{str_lit->value});
                op(Op::LOAD_STATIC, index);
                return;
            }
            if (auto b = ast::as<ast::Ident>(expr)) {
                auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(expr));
                auto node = base->first->base.lock();
                if (auto field = ast::as<ast::Field>(node)) {
                    if (field->is_state_variable) {
                        auto& info = state_variable_info[ast::cast_to<ast::Field>(node)];
                        op(Op::LOAD_IMMEDIATE, info.offset);
                        op(Op::LOAD_VARIABLE, TransferArg(0, 0));
                    }
                    else {
                        auto& info = field_info[ast::cast_to<ast::Field>(node)];
                        op(Op::LOAD_IMMEDIATE, info.offset);
                        op(Op::GET_FIELD, TransferArg(this_register, 0, 0));
                    }
                    return;
                }
            }
            if (auto a = ast::as<ast::MemberAccess>(expr)) {
                compile_expr(a->target);
                auto base = ast::tool::lookup_base(a->member);
                auto node = base->first->base.lock();
                if (auto field = ast::as<ast::Field>(node)) {
                    auto& info = field_info[ast::cast_to<ast::Field>(node)];
                    op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                    op(Op::LOAD_IMMEDIATE, info.offset);
                    // registers[0] = registers[1][registers[0]]
                    op(Op::GET_FIELD, TransferArg(1, 0, 0));
                    return;
                }
            }
            if (auto b = ast::as<ast::Binary>(expr)) {
                switch (b->op) {
                    case ast::BinaryOp::logical_and: {
                        compile_expr(b->left);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, 0);
                        op(Op::CMP, TransferArg(0, 1));  // compare register 0 and 1
                        auto idx = op(Op::JNE);
                        compile_expr(b->right);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, 0);
                        op(Op::CMP, TransferArg(0, 1));  // compare register 0 and 1
                        auto idx2 = op(Op::JNE);
                        op(Op::LOAD_IMMEDIATE, 1);
                        auto idx3 = op(Op::JMP);
                        rewrite_arg(idx, pc());
                        rewrite_arg(idx2, pc());
                        op(Op::LOAD_IMMEDIATE, 0);
                        rewrite_arg(idx3, pc());
                        return;
                    }
                    case ast::BinaryOp::logical_or: {
                        compile_expr(b->left);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, 0);
                        op(Op::CMP, TransferArg(0, 1));  // compare register 0 and 1
                        auto idx = op(Op::JE);
                        compile_expr(b->right);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, 0);
                        op(Op::CMP, TransferArg(0, 1));  // compare register 0 and 1
                        auto idx2 = op(Op::JE);
                        op(Op::LOAD_IMMEDIATE, 0);
                        auto idx3 = op(Op::JMP);
                        rewrite_arg(idx, pc());
                        rewrite_arg(idx2, pc());
                        op(Op::LOAD_IMMEDIATE, 1);
                        rewrite_arg(idx3, pc());
                        return;
                    }
                    default:
                        break;
                }
                compile_expr(b->left);
                op(Op::PUSH, 0);  // push register 0 to stack
                compile_expr(b->right);
                op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                op(Op::POP, 0);
                switch (b->op) {
                    case ast::BinaryOp::add:
                        op(Op::ADD, TransferArg(0, 1));
                        break;
                    case ast::BinaryOp::sub:
                        op(Op::SUB, TransferArg(0, 1));
                        break;
                    case ast::BinaryOp::mul:
                        op(Op::MUL, TransferArg(0, 1));
                        break;
                    case ast::BinaryOp::div:
                        op(Op::DIV, TransferArg(0, 1));
                        break;
                    case ast::BinaryOp::mod:
                        op(Op::MOD, TransferArg(0, 1));
                        break;
                    case ast::BinaryOp::equal:
                    case ast::BinaryOp::not_equal: {
                        op(Op::CMP, TransferArg(0, 1));
                        auto eq_not_eq = b->op == ast::BinaryOp::equal ? Op::JE : Op::JNE;
                        auto idx = op(eq_not_eq);
                        op(Op::LOAD_IMMEDIATE, 0);
                        auto idx2 = op(Op::JMP);
                        rewrite_arg(idx, pc());
                        op(Op::LOAD_IMMEDIATE, 1);
                        rewrite_arg(idx2, pc());
                        break;
                    }
                    default: {
                        auto index = add_static(Value{futils::view::rvec("error: unknown binary operator")});
                        op(Op::LOAD_STATIC, index);
                        op(Op::ERROR);
                        break;
                    }
                }
            }
        }

        void compile_byte_array_decode(const std::shared_ptr<ast::Field>& f, ast::ArrayType* arr_ty) {
            if (arr_ty->length_value) {
                op(Op::LOAD_IMMEDIATE, *arr_ty->length_value);
                op(Op::READ_BYTES);
            }
            else if (auto l = ast::as<ast::Range>(arr_ty->length);
                     l && !l->start && !l->end) {
                if (!f) {
                    auto index = add_static(Value{futils::view::rvec("error: array length is not decidable")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                }
                else if (f->eventual_follow == ast::Follow::end) {
                    auto tail = f->belong_struct.lock()->fixed_tail_size;
                    op(Op::GET_INPUT_REMAIN, 1);
                    op(Op::LOAD_IMMEDIATE, tail);
                    op(Op::CMP, TransferArg(1, 0));
                    auto instr = op(Op::JL);  // if input remain < tail then jump to error
                    op(Op::GET_INPUT_REMAIN, 1);
                    op(Op::LOAD_IMMEDIATE, tail);
                    op(Op::SUB, TransferArg(1, 0));  // input remain - tail
                    op(Op::READ_BYTES);
                    auto instr2 = op(Op::JMP);  // jump to end of array decode
                    rewrite_arg(instr, pc());   // rewrite jump to error
                    auto index = add_static(Value{futils::view::rvec("error: input remain is not enough")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                    rewrite_arg(instr2, pc());  // rewrite jump to end of array decode
                }
                else {
                    auto index = add_static(Value{futils::view::rvec("error: array length is not decidable")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                }
            }
            else {
                compile_expr(arr_ty->length);
                op(Op::READ_BYTES);
            }
        }

        void compile_complex_array(const std::shared_ptr<ast::Field>& f, ast::ArrayType* arr_ty) {
            auto elm = arr_ty->element_type;
            if (auto l = ast::as<ast::Range>(arr_ty->length);
                l && !l->start && !l->end) {
                if (!f) {
                    auto index = add_static(Value{futils::view::rvec("error: array length is not decidable")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                }
                else if (f->eventual_follow == ast::Follow::end) {
                    auto tail = f->belong_struct.lock()->fixed_tail_size;
                    op(Op::LOAD_IMMEDIATE, 0);
                    op(Op::PUSH, this_register);                          // save this_register
                    op(Op::MAKE_ARRAY, TransferArg(0, this_register));    // make empty array to this_register
                    op(Op::LOAD_IMMEDIATE, 0);                            // load 0 to register 0
                    op(Op::TRSF, TransferArg(0, 2));                      // register 0 to 2. register 2 is index of array
                    auto loop_start = op(Op::GET_INPUT_REMAIN, 1);        // get input remain to register 1
                    op(Op::LOAD_IMMEDIATE, tail);                         // load tail size to register 0
                    op(Op::CMP, TransferArg(1, 0));                       // compare input remain and tail size
                    auto instr = op(Op::JLE);                             // if input remain <= tail then jump to end of array decode
                    op(Op::PUSH, 1);                                      // push register 1 to stack
                    op(Op::PUSH, 2);                                      // push register 2 to stack
                    compile_decode_type(nullptr, elm);                    // decode element type
                    op(Op::POP, 2);                                       // pop stack to register 2
                    op(Op::POP, 1);                                       // pop stack to register 1
                    op(Op::SET_ARRAY, TransferArg(0, this_register, 2));  // this_register[register 2] = register 0
                    op(Op::INC, 2);                                       // register 2 += 1
                    op(Op::JMP, loop_start);                              // jump to loop start
                    rewrite_arg(instr, pc());                             // rewrite jump to end of array decode
                }
                else {
                    auto index = add_static(Value{futils::view::rvec("error: array length is not decidable")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                }
            }
            else {
                if (arr_ty->length_value) {
                    op(Op::LOAD_IMMEDIATE, *arr_ty->length_value);
                }
                else {
                    compile_expr(arr_ty->length);
                }
                op(Op::TRSF, TransferArg(0, 1));                      // register 0 to 1
                op(Op::PUSH, this_register);                          // save this_register
                op(Op::MAKE_ARRAY, TransferArg(1, this_register));    // make array to this_register with length of register 1
                op(Op::LOAD_IMMEDIATE, 0);                            // load 0 to register 0
                op(Op::TRSF, TransferArg(0, 2));                      // register 0 to 2
                auto loop_start = op(Op::CMP, TransferArg(1, 2));     // compare register 1 and 2
                auto brk = op(Op::JE);                                // if register 1 == register 2 then jump to end of array decode
                op(Op::PUSH, 1);                                      // push register 1 to stack
                op(Op::PUSH, 2);                                      // push register 2 to stack
                compile_decode_type(nullptr, elm);                    // decode element type
                op(Op::POP, 2);                                       // pop stack to register 2
                op(Op::POP, 1);                                       // pop stack to register 1
                op(Op::SET_ARRAY, TransferArg(0, this_register, 2));  // this_register[register 2] = register 0
                op(Op::INC, 2);                                       // register 2 += 1
                op(Op::JMP, loop_start);                              // jump to loop start
                rewrite_arg(brk, pc());                               // rewrite jump to end of array decode
                op(Op::TRSF, TransferArg(this_register, 0));          // this_register to register 0
                op(Op::POP, this_register);                           // restore this_register
            }
        }

        void compile_decode_type(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::Type>& base_ty) {
            std::shared_ptr<ast::Type> typ = base_ty;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                auto size = *int_ty->bit_size;
                if (size % 8 == 0) {
                    op(Op::LOAD_IMMEDIATE, size / 8);
                    op(Op::READ_BYTES);
                    if (int_ty->endian != ast::Endian::unspec) {
                        op(Op::SET_ENDIAN, int_ty->endian == ast::Endian::big ? 0 : 1);
                    }
                    op(Op::BYTES_TO_INT, size / 8);
                }
                else {
                    op(Op::LOAD_IMMEDIATE, size);
                    op(Op::READ_BITS);
                    if (int_ty->endian != ast::Endian::unspec) {
                        op(Op::SET_ENDIAN, int_ty->endian == ast::Endian::big ? 0 : 1);
                    }
                    op(Op::BYTES_TO_INT, (size + 7) / 8);
                }
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                if (auto u8 = ast::as<ast::IntType>(arr_ty->element_type); u8 && u8->bit_size == 8) {
                    compile_byte_array_decode(f, arr_ty);
                }
                else {
                    compile_complex_array(f, arr_ty);
                }
            }
            if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
                op(Op::LOAD_IMMEDIATE, str_ty->strong_ref->length);
                op(Op::READ_BYTES);
                auto str = *unescape(str_ty->strong_ref->value);
                auto index = add_static(Value{std::move(str)});
                op(Op::LOAD_STATIC, index);
                op(Op::CMP, TransferArg(0, 1));
                auto instr = op(Op::JE);
                index = add_static(Value{brgen::concat("error: expect ", str_ty->strong_ref->value, " but not")});
                op(Op::LOAD_STATIC, index);
                op(Op::ERROR);
                rewrite_arg(instr, pc());
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ)) {
                auto base = struct_ty->base.lock();
                if (auto fmt = ast::as<ast::Format>(base)) {
                    auto& info = format_info[ast::cast_to<ast::Format>(base)];
                    op(Op::MAKE_OBJECT, 0);
                    op(Op::PUSH, this_register);                  // save this_register
                    op(Op::TRSF, TransferArg(0, this_register));  // register 0 to this_register
                    op(Op::LOAD_IMMEDIATE, info.decode_entry);    // set decode function address
                    op(Op::CALL, 0);                              // register[0]() call decode function
                    op(Op::TRSF, TransferArg(this_register, 0));  // this_register to register 0
                    op(Op::POP, this_register);                   // restore this_register
                }
            }
        }

        void compile_decode_field(const std::shared_ptr<ast::Format>& fmt, const std::shared_ptr<ast::Field>& field) {
            compile_decode_type(field, field->field_type);
            // register 0 to 1
            op(Op::TRSF, TransferArg(0, 1));
            // load offset to register 0
            op(Op::LOAD_IMMEDIATE, field_info[field].offset);
            // from register 1 to this_register object field referenced by offset of register 0 value
            op(Op::SET_FIELD, TransferArg(1, this_register, 0));
            auto index = add_static(Value{field->ident->ident});
            op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
            op(Op::LOAD_STATIC, index);
            op(Op::SET_FIELD_LABEL, TransferArg(0, this_register, 1));
        }

        void compile_field(const std::shared_ptr<ast::Format>& fmt, const std::shared_ptr<ast::Field>& field) {
            if (field_info.find(field) == field_info.end()) {
                auto& info = format_info[fmt];
                field_info[field] = FieldInfo{fmt, info.current_offset};
                info.current_offset += 1;
            }
            if (encode) {
                // compile_encode_field(fmt, field);
            }
            else {
                compile_decode_field(fmt, field);
            }
        }

        void compile_block(const std::shared_ptr<ast::Format>& fmt, const std::shared_ptr<ast::IndentBlock>& block) {
            for (auto& element : block->elements) {
                if (auto field = ast::as<ast::Field>(element)) {
                    compile_field(fmt, ast::cast_to<ast::Field>(element));
                }
                if (auto as = ast::as<ast::Assert>(element)) {
                    compile_expr(as->cond);
                    op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                    op(Op::LOAD_IMMEDIATE, 0);
                    op(Op::CMP, TransferArg(0, 1));
                    auto instr = op(Op::JNE);
                    auto index = add_static(Value{futils::view::rvec("error: assert failed")});
                    op(Op::LOAD_STATIC, index);
                    op(Op::ERROR);
                    rewrite_arg(instr, pc());
                }
                if (auto if_ = ast::as<ast::If>(element)) {
                    auto elif_ = if_;
                    size_t instr = 0, instr2 = 0;
                    std::vector<size_t> jump;
                    while (elif_) {
                        compile_expr(elif_->cond);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, 0);
                        op(Op::CMP, TransferArg(0, 1));
                        instr = op(Op::JE);
                        compile_block(fmt, elif_->then);
                        jump.push_back(op(Op::JMP));
                        rewrite_arg(instr, pc());  // next if or else or end of if-elif-else
                        auto next = ast::as<ast::If>(elif_->els);
                        if (!next) {
                            break;
                        }
                        elif_ = next;
                    }
                    if (auto block = ast::as<ast::IndentBlock>(elif_->els)) {
                        compile_block(fmt, ast::cast_to<ast::IndentBlock>(elif_->els));
                    }
                    for (auto i : jump) {  // rewrite all jump to end of if-elif-else
                        rewrite_arg(i, pc());
                    }
                }
            }
        }

        void compile_encode_format(const std::shared_ptr<ast::Format>& fmt) {
            const auto d = function_prologue(fmt->ident->ident + ".encode");
            encode = true;
            format_info[fmt].encode_entry = pc();
            compile_block(fmt, fmt->body);
        }

        void compile_decode_format(const std::shared_ptr<ast::Format>& fmt) {
            const auto d = function_prologue(fmt->ident->ident + ".decode");
            encode = false;
            format_info[fmt].decode_entry = pc();
            compile_block(fmt, fmt->body);
        }

        void compile_init_var(const std::shared_ptr<ast::Type>& b_typ) {
            auto typ = b_typ;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto i_typ = ast::as<ast::IntType>(typ)) {
                op(Op::LOAD_IMMEDIATE, 0);
            }
            if (auto s = ast::as<ast::StructType>(typ)) {
                auto base = s->base.lock();
                op(Op::MAKE_OBJECT, this_register);
                size_t i = 0;
                for (auto& field : s->fields) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        field_info[ast::cast_to<ast::Field>(field)].offset = i;
                        op(Op::PUSH, this_register);
                        compile_init_var(f->field_type);
                        op(Op::POP, this_register);
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_IMMEDIATE, i);
                        op(Op::SET_FIELD, TransferArg(1, this_register, 0));
                        auto index = add_static(Value{f->ident->ident});
                        op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                        op(Op::LOAD_STATIC, index);
                        op(Op::SET_FIELD_LABEL, TransferArg(0, this_register, 1));
                        i++;
                    }
                }
                op(Op::TRSF, TransferArg(this_register, 0));
            }
            if (auto b = ast::as<ast::BoolType>(typ)) {
                op(Op::LOAD_IMMEDIATE, 0);
            }
        }

        void compile_format(const std::shared_ptr<ast::Format>& fmt) {
            if (format_info.find(fmt) != format_info.end()) {
                return;
            }
            format_info[fmt] = FormatInfo{};
            for (auto& dep : fmt->depends) {
                auto l = dep.lock();
                if (l) {
                    auto s = l->base.lock();
                    if (auto st = ast::as<ast::StructType>(s)) {
                        auto maybe_fmt = st->base.lock();
                        if (auto fmt2 = ast::as<ast::Format>(maybe_fmt)) {
                            compile_format(ast::cast_to<ast::Format>(maybe_fmt));
                        }
                    }
                }
            }
            for (auto& state_variable : fmt->state_variables) {
                auto f = state_variable.lock();
                if (state_variable_info.find(f) == state_variable_info.end()) {
                    state_variable_info[f].offset = state_variable_offset++;
                    op(Op::INIT_VARIABLE, state_variable_offset - 1);
                    compile_init_var(f->field_type);
                    op(Op::TRSF, TransferArg(0, 1));  // register 0 to 1
                    op(Op::LOAD_IMMEDIATE, state_variable_offset - 1);
                    op(Op::STORE_VARIABLE, TransferArg(1, 0));
                }
            }
            compile_encode_format(fmt);
            compile_decode_format(fmt);
        }

        void compile(const std::shared_ptr<ast::Program>& prog) {
            for (auto& element : prog->elements) {
                if (auto fmt = ast::as<ast::Format>(element)) {
                    compile_format(ast::cast_to<ast::Format>(element));
                }
            }
        }
    };

    Code compile(const std::shared_ptr<ast::Program>& prog) {
        Compiler compiler;
        compiler.compile(prog);
        return compiler.code;
    }

    void print_code(futils::helper::IPushBacker<> out, const Code& code) {
        futils::code::CodeWriter<decltype(out)&> w{out};
        w.writeln("instruction: ", brgen::nums(code.instructions.size()));
        auto s1 = w.indent_scope();
        for (size_t i = 0; i < code.instructions.size(); i++) {
            auto& instr = code.instructions[i];
            w.write(brgen::nums(i), " ", to_string(instr.op()), " 0x", brgen::nums(instr.arg(), 16));
            if (instr.op() == Op::FUNC_NAME) {
                auto d = code.static_data[instr.arg()].as_bytes();
                if (d) {
                    w.write(" # ", futils::escape::escape_str<std::string>(*d, futils::escape::EscapeFlag::all));
                }
            }
            w.writeln();
        }
        s1.execute();
        w.writeln("static data: ", brgen::nums(code.static_data.size()));
        auto s2 = w.indent_scope();
        for (size_t i = 0; i < code.static_data.size(); i++) {
            auto& data = code.static_data[i];
            if (auto b = data.as_bytes()) {
                w.writeln(brgen::nums(i), " \"", futils::escape::escape_str<std::string>(*b, futils::escape::EscapeFlag::all), "\"");
            }
            else if (auto n = data.as_uint64()) {
                w.writeln(brgen::nums(i), " ", brgen::nums(*n));
            }
        }
        s2.execute();
    }
}  // namespace brgen::vm
