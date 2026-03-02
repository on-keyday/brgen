/*license*/
#pragma once

#include <vector>
#include "ebm/extended_binary_module.hpp"
#include "helper/defer.h"
namespace ebm2rmw {

    struct Instruction {
        ebm::Instruction instr;
        std::string str_repr;
    };

    struct Env {
       private:
        std::vector<Instruction>* instructions;

        std::map<std::uint64_t, std::vector<Instruction>> functions;
        std::vector<std::uint64_t> function_insert_order;

       public:
        const std::vector<Instruction>& get_instructions() const {
            return *instructions;
        }

        std::vector<Instruction>& access_instructions() {
            return *instructions;
        }

        void add_instruction(const ebm::Instruction& instr, std::string str_repr) {
            instructions->push_back(Instruction{
                .instr = instr,
                .str_repr = std::move(str_repr),
            });
        }

        bool has_function(ebm::StatementRef func_id) const {
            return functions.find(get_id(func_id)) != functions.end();
        }

        auto new_function(ebm::StatementRef func_id) {
            auto old = instructions;
            if (!has_function(func_id)) {
                functions[get_id(func_id)] = {};
                function_insert_order.push_back(get_id(func_id));
            }
            instructions = &functions[get_id(func_id)];
            return futils::helper::defer([&, old] {
                instructions = old;
            });
        }

        std::map<std::uint64_t, std::vector<Instruction>>& get_functions() {
            return functions;
        }

        std::vector<std::uint64_t>& get_function_insert_order() {
            return function_insert_order;
        }
    };
}  // namespace ebm2rmw
