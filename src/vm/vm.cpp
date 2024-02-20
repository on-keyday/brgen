/*license*/
#include "vm.h"
#include <core/common/util.h>
#include <binary/reader.h>

namespace brgen::vm {

    struct VMHelper {
        static bool arithmetic_op(VM& vm, auto&& cb) {
            auto a = vm.registers[0].as_uint64();
            auto b = vm.registers[1].as_uint64();
            if (!a || !b) {
                return false;
            }
            std::optional<std::uint64_t> result = cb(*a, *b);
            if (!result) {
                return false;
            }
            vm.registers[0] = Value(*result);
            return true;
        }

        static bool add(VM& vm) {
            return arithmetic_op(vm, [](auto a, auto b) { return a + b; });
        }

        static bool sub(VM& vm) {
            return arithmetic_op(vm, [](auto a, auto b) { return a - b; });
        }

        static bool mul(VM& vm) {
            return arithmetic_op(vm, [](auto a, auto b) { return a * b; });
        }

        static bool div(VM& vm) {
            return arithmetic_op(vm, [](auto a, auto b) -> std::optional<std::uint64_t> {
                if (b == 0) {
                    return std::nullopt;
                }
                return a / b;
            });
        }

        static bool mod(VM& vm) {
            return arithmetic_op(vm, [](auto a, auto b) -> std::optional<std::uint64_t> {
                if (b == 0) {
                    return std::nullopt;
                }
                return a % b;
            });
        }

        static bool compare_op(VM& vm, auto&& cb) {
            auto a = vm.registers[0].as_uint64();
            auto b = vm.registers[1].as_uint64();
            if (!a || !b) {
                auto a_bytes = vm.registers[0].as_bytes();
                auto b_bytes = vm.registers[1].as_bytes();
                if (!a_bytes || !b_bytes) {
                    return false;
                }
                vm.registers[0] = Value(cb(*a_bytes, *b_bytes));
                return true;
            }
            vm.registers[0] = Value(cb(*a, *b));
            return true;
        }

        static bool cmp(VM& vm) {
            return compare_op(vm, [](auto a, auto b) {
                if (a == b) {
                    return std::uint64_t(0);
                }
                if (a < b) {
                    return std::uint64_t(1);
                }
                return std::uint64_t(2);
            });
        }

        static bool transfer(VM& vm, const Instruction& instr) {
            auto arg = instr.arg();
            if (arg > 0xff) {
                return false;
            }
            auto from = (arg & 0xf0) >> 4;
            auto to = arg & 0x0f;
            if (from == to) {
                return true;  // no-op
            }
            vm.registers[to] = vm.registers[from];
            return true;
        }

        static bool init_var(VM& vm, const Instruction& instr) {
            auto indexOfVar = instr.arg();
            if (vm.variables.size() <= indexOfVar) {
                return false;
            }
            auto value = vm.registers[0];
            auto label = vm.registers[1].as_bytes();
            if (!label) {
                vm.variables[indexOfVar] = Var("", std::move(value));
            }
            else {
                vm.variables[indexOfVar] = Var(std::string(label->as_char(), label->size()), std::move(value));
            }
            return true;
        }

        static bool store_var(VM& vm, const Instruction& instr) {
            auto indexOfVar = instr.arg();
            if (vm.variables.size() <= indexOfVar) {
                return false;
            }
            auto value = vm.registers[0];
            vm.variables[indexOfVar].value(std::move(value));
            return true;
        }

        static bool load_var(VM& vm, const Instruction& instr) {
            auto indexOfVar = instr.arg();
            if (vm.variables.size() <= indexOfVar) {
                return false;
            }
            vm.registers[0] = vm.variables[indexOfVar].value();
            return true;
        }

        static bool static_load(VM& vm, const Instruction& instr, const std::vector<Value>& static_data) {
            auto indexOfStatic = instr.arg();
            if (static_data.size() <= indexOfStatic) {
                return false;
            }
            vm.registers[0] = static_data[indexOfStatic];
            return true;
        }

        static bool push(VM& vm) {
            vm.stack.push_back(vm.registers[0]);
            return true;
        }

        static bool pop(VM& vm) {
            if (vm.stack.empty()) {
                return false;
            }
            vm.registers[0] = std::move(vm.stack.back());
            vm.stack.pop_back();
            return true;
        }

        static bool get_offset(VM& vm, futils::binary::reader& r) {
            vm.registers[0] = Value(r.offset());
            return true;
        }

        static bool set_offset(VM& vm, futils::binary::reader& r) {
            auto offset = vm.registers[0].as_uint64();
            if (!offset) {
                return false;
            }
            auto ofs = *offset;
            r.reset(0);
            if (r.remain().size() < ofs) {
                return false;
            }
            r.reset(ofs);
            return true;
        }
    };

    void VM::execute(const std::vector<Instruction>& program, const std::vector<Value>& static_data) {
        size_t pc = 0;
        futils::binary::reader r{input};
        while (pc < program.size()) {
            const auto& instr = program[pc];
            switch (instr.op()) {
                case Op::NOP:
                    break;
#define exec_op(code, op, ...)                                 \
    case code: {                                               \
        if (!VMHelper::op(*this __VA_OPT__(, ) __VA_ARGS__)) { \
            error_message = #code " failed";                   \
            return;                                            \
        }                                                      \
        break;                                                 \
    }

                    exec_op(Op::ADD, add);
                    exec_op(Op::SUB, sub);
                    exec_op(Op::MUL, mul);
                    exec_op(Op::DIV, div);
                    exec_op(Op::MOD, mod);

                    exec_op(Op::CMP, cmp);

                    exec_op(Op::TRSF, transfer, instr);

                    exec_op(Op::INIT_VAR, init_var, instr);
                    exec_op(Op::LOAD_VAR, load_var, instr);
                    exec_op(Op::STORE_VAR, store_var, instr);
                    exec_op(Op::STATIC_LOAD, static_load, instr, static_data);

                    exec_op(Op::PUSH, push);
                    exec_op(Op::POP, pop);

                    exec_op(Op::GET_OFFSET, get_offset, r);
                    exec_op(Op::SET_OFFSET, set_offset, r);

                default: {
                    auto ptr = to_string(instr.op());
                    if (ptr) {
                        error_message = "Unimplemented opcode: " + std::string(ptr);
                    }
                    else {
                        error_message = "Unknown opcode: " + brgen::nums(static_cast<int>(instr.op()));
                    }
                    return;
                }
            }
            pc++;
        }
    }
}  // namespace brgen::vm
