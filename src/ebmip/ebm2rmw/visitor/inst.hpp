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
        ebm::TypeRef type_info;
    };

    struct FunctionDecl {
        std::vector<Instruction> instructions;
        size_t local_count = 0;
        std::unordered_map<ebm::StatementRef, size_t> local_indices;
        size_t param_count = 0;
        std::unordered_map<ebm::StatementRef, size_t> param_indices;
        size_t structs_area = 0;
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

        size_t struct_area_offset(size_t size) {
            auto& func = *instructions;
            size_t offset = func.structs_area;
            func.structs_area += size;
            return offset;
        }

        size_t get_local(ebm::StatementRef local_id) const {
            auto& func = *instructions;
            auto found = func.local_indices.find(local_id);
            assert(found != func.local_indices.end());
            return found->second;
        }

        void add_param(ebm::StatementRef param_id) {
            auto& func = *instructions;
            if (func.param_indices.find(param_id) == func.param_indices.end()) {
                func.param_indices[param_id] = func.param_count++;
            }
        }

        size_t get_param(ebm::StatementRef param_id) const {
            auto& func = *instructions;
            auto found = func.param_indices.find(param_id);
            assert(found != func.param_indices.end());
            return found->second;
        }

        void add_instruction(const ebm::Instruction& instr, std::string str_repr, std::uint64_t scratch = 0, ebm::TypeRef type_info = {}) {
            instructions->instructions.push_back(Instruction{
                .instr = instr,
                .str_repr = std::move(str_repr),
                .scratch = scratch,
                .type_info = type_info,
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
