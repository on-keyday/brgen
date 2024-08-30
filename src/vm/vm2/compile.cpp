/*license*/
#include "vm2.h"
#include <core/ast/ast.h>
#include <unordered_map>
#include "layout.h"

namespace brgen::vm2 {

    struct InstCodeMap {
        Inst inst;
        size_t code_offset = 0;
        size_t end_offset = 0;
    };

    struct Relocation {
        size_t instruction_offset;
        size_t rewrite_to_offset;
        size_t rewrite_arg = 0;
    };

    struct FunctionRelocation {
        std::string name;
        size_t instruction_offset;
        size_t rewrite_to_offset;
    };

    struct Compiler2 {
        std::vector<InstCodeMap> insts;
        std::vector<Relocation> relocations;
        std::vector<FunctionRelocation> function_relocations;
        std::unordered_map<std::string, std::uint64_t> function_addresses;

        TypeContext type_context;

        size_t op(Inst in) {
            insts.push_back({in, 0});
            return insts.size() - 1;
        }

        void relocate(size_t instruction_offset, size_t rewrite_to_offset, size_t rewrite_arg) {
            relocations.push_back({instruction_offset, rewrite_to_offset, rewrite_arg});
        }

        void relocate_function(const std::string& name, size_t instruction_offset, size_t rewrite_to_offset) {
            function_relocations.push_back({name, instruction_offset, rewrite_to_offset});
        }

        // when allocation fails, program will be terminated
        void allocate_to_r0(size_t size) {
            op(inst::load_immediate(Register::R1, size));
            op(inst::syscall(SyscallNumber::ALLOCATE));
        }

        void allocate_dynamic_object(Register size_reg) {
            op(inst::transfer(size_reg, Register::R1));
            op(inst::syscall(SyscallNumber::ALLOCATE));
        }

        void new_object(size_t size) {
            allocate_to_r0(size);
            op(inst::push(Register::OBJECT_POINTER));
            op(inst::transfer(Register::R0, Register::OBJECT_POINTER));
        }

        void return_object() {
            op(inst::transfer(Register::OBJECT_POINTER, Register::R0));
            op(inst::pop(Register::OBJECT_POINTER));
        }

        void read_n_bit(Register buffer_ptr, size_t buffer_size, size_t n) {
            op(inst::transfer(buffer_ptr, Register::R1));
            op(inst::load_immediate(Register::R2, buffer_size));
            op(inst::load_immediate(Register::R3, n));
            op(inst::syscall(SyscallNumber::READ_IN));
        }

        void write_n_bit(Register buffer_ptr, size_t buffer_size, size_t n) {
            op(inst::transfer(buffer_ptr, Register::R1));
            op(inst::load_immediate(Register::R2, buffer_size));
            op(inst::load_immediate(Register::R3, n));
            op(inst::syscall(SyscallNumber::WRITE_OUT));
        }

        void object_address(Register reg, size_t offset) {
            op(inst::load_immediate(reg, offset));
            op(inst::add(Register::OBJECT_POINTER, reg, reg));
        }

        void object_store(Register src, Register dst, size_t size, size_t offset) {
            object_address(dst, offset);
            op(inst::store_memory(src, dst, size));
        }

        // this modify R0, R1, R2
        void read_n_bit_integer_to_r0(size_t n) {
            auto len = n / 64 + (n % 64 != 0);
            for (size_t i = 0; i < len; i++) {
                op(inst::push_immediate(0));
            }
            read_n_bit(Register::SP, len * 8, n);
            op(inst::syscall(SyscallNumber::BTOI));
            for (size_t i = 0; i < len; i++) {
                op(inst::pop(Register::NUL));
            }
        }

        void write_n_byte_integer_from_r0(Register integer, size_t n) {
            auto len = n / 64 + (n % 64 != 0);
            for (size_t i = 0; i < len; i++) {
                op(inst::push_immediate(0));
            }
            op(inst::transfer(Register::SP, Register::R1));
            op(inst::load_immediate(Register::R2, n));
            op(inst::transfer(integer, Register::R3));
            op(inst::syscall(SyscallNumber::ITOB));
            write_n_bit(Register::SP, len * 8, n);
            for (size_t i = 0; i < len; i++) {
                op(inst::pop(Register::NUL));
            }
        }

        void for_loop(Register counter, Register limit, auto&& f) {
            op(inst::load_immediate(counter, 0));               // counter = 0
            op(inst::push(counter));                            // push(counter)
            op(inst::push(limit));                              // push(limit)
            auto loop = op(inst::nop());                        // loop:
            op(inst::load_memory(Register::SP, limit, 8));      // limit = [SP]
            op(inst::load_immediate(counter, 8));               // counter = 8
            op(inst::add(Register::SP, counter, counter));      // counter = SP + counter = SP + 8
            op(inst::load_memory(counter, counter, 8));         // counter = [counter] (= [SP + 8])
            f();                                                // f()
            op(inst::pop(limit));                               // limit = pop()
            op(inst::pop(counter));                             // counter = pop()
            op(inst::inc(counter, counter));                    // counter++
            op(inst::push(counter));                            // push(counter)
            op(inst::lt(counter, limit, counter));              // counter < limit
            op(inst::push(limit));                              // push(limit)
            auto addr = limit;                                  // rename to addr
            auto cond = counter;                                // rename to cond
            auto jmp_addr = op(inst::load_immediate(addr, 0));  // addr = loop
            relocate(jmp_addr, loop, 2);                        //
            op(inst::jump_if(addr, cond));                      // if (cond) goto addr
            op(inst::pop(Register::NUL));                       // pop()
            op(inst::pop(Register::NUL));                       // pop()
        }

        void compile_decode_type(size_t offset, size_t size, const std::shared_ptr<ast::Type>& typ) {
            if (auto ity = ast::as<ast::IntType>(typ)) {
                read_n_bit_integer_to_r0(*ity->bit_size);                // R0 = read_n_bit_integer(*typ->bit_size)
                object_store(Register::R0, Register::R1, size, offset);  // [OBJECT_POINTER + offset] = R0
            }
            else if (auto arr_typ = ast::as<ast::ArrayType>(typ)) {
                auto array = std::static_pointer_cast<Array>(type_context.compute_layout(typ));
                compile_expr(arr_typ->length);
                auto ptr_reg = Register::R0;
                auto length_reg = Register::R1;
                auto element_size_reg = Register::R2;
                auto byte_size_reg = Register::R3;
                auto tmp_reg = Register::R4;
                op(inst::transfer(Register::R0, length_reg));                                    // length_reg = R0
                op(inst::load_immediate(element_size_reg, array->element->size));                // element_size_reg = element_size
                op(inst::mul(length_reg, element_size_reg, byte_size_reg));                      // byte_size_reg = length_reg * element_size_reg
                allocate_dynamic_object(byte_size_reg);                                          // ptr_reg = allocate_dynamic_object(byte_size_reg)
                object_store(ptr_reg, tmp_reg, size / 2, offset);                                // [ptr_reg + offset] = ptr_reg
                object_store(element_size_reg, tmp_reg, size / 2, offset + size / 2);            // [ptr_reg + offset + size / 2] = element_size_reg
                op(inst::push(Register::OBJECT_POINTER));                                        // push(OBJECT_POINTER)
                op(inst::transfer(ptr_reg, Register::OBJECT_POINTER));                           // OBJECT_POINTER = ptr_reg
                for_loop(tmp_reg, element_size_reg, [&] {                                        // for (tmp_reg = 0; tmp_reg < element_size_reg; tmp_reg++)
                    op(inst::push(Register::OBJECT_POINTER));                                    // push(OBJECT_POINTER)
                    op(inst::mul(tmp_reg, element_size_reg, tmp_reg));                           // tmp_reg = tmp_reg * element_size_reg
                    op(inst::add(Register::OBJECT_POINTER, tmp_reg, Register::OBJECT_POINTER));  // OBJECT_POINTER = OBJECT_POINTER + tmp_reg
                    op(inst::push(element_size_reg));                                            // push(element_size_reg)
                    compile_decode_type(0, array->element->size, arr_typ->element_type);         // compile_decode_type(0, element_size, arr_typ->element_type)
                    op(inst::pop(element_size_reg));                                             // element_size_reg = pop()
                    op(inst::pop(Register::OBJECT_POINTER));                                     // OBJECT_POINTER = pop()
                });                                                                              // end for
                op(inst::pop(Register::OBJECT_POINTER));                                         // OBJECT_POINTER = pop()
            }
            else if (auto s = ast::as<ast::IdentType>(typ)) {
                compile_decode_type(offset, size, s->base.lock());
            }
            else if (auto s = ast::as<ast::StructType>(typ)) {
                auto p = ast::as<ast::Member>(s->base.lock());
                auto addr = op(inst::load_immediate(Register::R1, 0));
                relocate_function(p->ident->ident + ".decode", addr, 1);
                op(inst::call(Register::R1));
                object_store(Register::R0, Register::R1, size, offset);
            }
        }

        void compile_expr(const std::shared_ptr<ast::Expr>& expr) {
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                auto p = lit->parse_as<std::uint64_t>();
                op(inst::load_immediate(Register::R0, *p));
            }
        }

        void compile_node(const std::shared_ptr<ast::Node>& node) {
            if (auto expr = ast::as<ast::IndentBlock>(node)) {
                for (auto& stmt : expr->elements) {
                    compile_node(stmt);
                }
            }
            else if (auto s = ast::as<ast::Field>(node)) {
                auto field = type_context.get_field(ast::cast_to<ast::Field>(node));
                compile_decode_type(field->offset, field->element->size, field->element->type_ast);
            }
        }

        void compile_decode(const std::shared_ptr<Struct>& s, const std::shared_ptr<ast::Format>& fmt) {
            auto decode_fn_entry = op(inst::func_entry());
            function_addresses[fmt->ident->ident + ".decode"] = decode_fn_entry;
            new_object(s->size);
            auto d = futils::helper::defer([&] { return_object(); });
            compile_node(fmt->body);
        }

        void compile(const std::shared_ptr<ast::Program>& prog) {
            for (auto& stmt : prog->elements) {
                if (auto fmt = ast::as<ast::Format>(stmt)) {
                    auto layout = type_context.compute_layout(fmt->body->struct_type);
                    compile_decode(std::static_pointer_cast<Struct>(layout), ast::cast_to<ast::Format>(stmt));
                }
            }
        }

        void encode(futils::binary::writer& w) {
            for (auto& reloc : function_relocations) {
                auto addr = function_addresses[reloc.name];
                relocate(reloc.instruction_offset, addr, reloc.rewrite_to_offset);
            }
            function_relocations.clear();
            for (auto& inst : insts) {
                inst.code_offset = w.offset();
                enc::encode(w, inst.inst);
                inst.end_offset = w.offset();
            }
            auto end = w.offset();
            for (auto& reloc : relocations) {
                auto ins = insts[reloc.instruction_offset];
                w.reset(ins.code_offset);
                auto rewritten = ins.inst.rewrite_arg(reloc.rewrite_arg, insts[reloc.rewrite_to_offset].code_offset);
                enc::encode(w, rewritten);
                assert(w.offset() == ins.end_offset);
            }
            w.reset(end);
        }
    };

    void compile(const std::shared_ptr<ast::Program>& node, futils::binary::writer& dst) {
        Compiler2 c;
        c.compile(node);
        c.encode(dst);
    }

}  // namespace brgen::vm2
