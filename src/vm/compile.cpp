/*license*/
#pragma once
#include "vm.h"
#include <core/ast/ast.h>
#include <core/ast/traverse.h>

namespace brgen::vm::compile {
    struct Compiler {
        std::vector<Instruction> instructions;
        std::vector<Value> static_data;

        auto function_prologue(const std::string& func_name) {
            auto indexOfInstruction = instructions.size();
            instructions.push_back(Instruction{Op::NEXT_FUNC, 0});
            auto indexOfFuncName = static_data.size();
            static_data.push_back(Value{func_name});
            instructions.push_back(Instruction{Op::FUNC_NAME, indexOfFuncName});
            return futils::helper::defer([&, indexOfInstruction] {
                instructions[indexOfInstruction].arg(instructions.size());
            });
        }

        void compile_encode_format(const std::shared_ptr<ast::Format>& fmt) {
            const auto d = function_prologue(fmt->ident->ident + ".encode");
        }

        void compile_decode_format(const std::shared_ptr<ast::Format>& fmt) {
            const auto d = function_prologue(fmt->ident->ident + ".decode");
        }

        void compile_format(const std::shared_ptr<ast::Format>& fmt) {
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
}  // namespace brgen::vm::compile
