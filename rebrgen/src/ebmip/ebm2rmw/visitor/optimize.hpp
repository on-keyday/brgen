/*license*/
#pragma once

#include "ebm/extended_binary_module.hpp"
#include "inst.hpp"
namespace ebm2rmw {
    inline std::optional<ebm::StatementRef> is_wrapper_function(std::vector<Instruction>& instructions) {
        if (instructions.size() != 3) {
            return std::nullopt;
        }
        if (instructions[0].instr.op == ebm::OpCode::LOAD_SELF_MEMBER &&
            instructions[1].instr.op == ebm::OpCode::CALL_DIRECT &&
            instructions[2].instr.op == ebm::OpCode::RET) {
            auto func_id = instructions[1].instr.func_id();
            if (func_id) {
                return *func_id;
            }
        }
        return std::nullopt;
    }

    struct Optimizer {
        void optimize_function(Env& env) {
            auto& functions = env.get_functions();
            std::unordered_map<ebm::StatementRef, ebm::StatementRef> wrapper_functions;
            for (auto& [func_id, func_decl] : functions) {
                if (auto wrapped = is_wrapper_function(func_decl.instructions)) {
                    wrapper_functions[func_id] = *wrapped;
                }
            }
            for (auto& [func_id, func_decl] : functions) {
                for (auto& instr : func_decl.instructions) {
                    if (auto func_id = instr.instr.func_id()) {
                        ebm::StatementRef target_func = *func_id;
                        for (;;) {
                            auto found = wrapper_functions.find(target_func);
                            if (found != wrapper_functions.end()) {
                                target_func = found->second;
                                continue;
                            }
                            break;
                        }
                        if (target_func != *func_id) {
                            instr.instr.func_id(target_func);
                        }
                    }
                }
            }
        }
    };
}  // namespace ebm2rmw