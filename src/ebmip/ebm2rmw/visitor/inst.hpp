/*license*/
#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>
#include "ebm/extended_binary_module.hpp"
#include "helper/defer.h"
namespace ebm2rmw {

    struct Instruction {
        ebm::Instruction instr;
        std::string str_repr;
        std::uint64_t scratch = 0;
    };

    struct FunctionDecl {
        std::vector<Instruction> instructions;
        size_t local_count = 0;
        std::unordered_map<ebm::StatementRef, size_t> local_indices;
        std::optional<size_t> error_slot;
    };

    struct Env {
       private:
        FunctionDecl* instructions;

        std::map<ebm::StatementRef, FunctionDecl> functions;
        std::vector<ebm::StatementRef> function_insert_order;

       public:
        const FunctionDecl* get_current_function() const {
            return instructions;
        }
        const std::vector<Instruction>& get_instructions() const {
            return instructions->instructions;
        }

        std::vector<Instruction>& access_instructions() {
            return instructions->instructions;
        }

        void add_local(ebm::StatementRef local_id) {
            auto& func = *instructions;
            if (func.local_indices.find(local_id) == func.local_indices.end()) {
                func.local_indices[local_id] = func.local_count++;
            }
        }

        size_t get_local(ebm::StatementRef local_id) const {
            auto& func = *instructions;
            auto found = func.local_indices.find(local_id);
            assert(found != func.local_indices.end());
            return found->second;
        }

        void add_error_slot(ebm::StatementRef error_id) {
            auto& func = *instructions;
            if (!func.error_slot) {
                func.error_slot = func.local_count++;
            }
            func.local_indices[error_id] = *func.error_slot;
        }

        void add_instruction(const ebm::Instruction& instr, std::string str_repr, std::uint64_t scratch = 0) {
            instructions->instructions.push_back(Instruction{
                .instr = instr,
                .str_repr = std::move(str_repr),
                .scratch = scratch,
            });
        }

        bool has_function(ebm::StatementRef func_id) const {
            return functions.find(func_id) != functions.end();
        }

        auto new_function(ebm::StatementRef func_id) {
            auto old = instructions;
            if (!has_function(func_id)) {
                functions[func_id] = {};
                function_insert_order.push_back(func_id);
            }
            instructions = &functions[func_id];
            return futils::helper::defer([&, old] {
                instructions = old;
            });
        }

        std::map<ebm::StatementRef, FunctionDecl>& get_functions() {
            return functions;
        }

        std::vector<ebm::StatementRef>& get_function_insert_order() {
            return function_insert_order;
        }
    };
}  // namespace ebm2rmw
