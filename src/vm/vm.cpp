/*license*/
#include "vm.h"
#include <core/common/util.h>
#include <binary/reader.h>
#include <number/insert_space.h>

namespace brgen::vm {

    struct VMHelper {
        static bool arithmetic_op(VM& vm, const Instruction& instr, auto&& cb) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto a = vm.registers[arg.from()].as_uint64();
            auto b = vm.registers[arg.to()].as_uint64();
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

        static bool add(VM& vm, const Instruction& instr) {
            return arithmetic_op(vm, instr, [](auto a, auto b) { return a + b; });
        }

        static bool sub(VM& vm, const Instruction& instr) {
            return arithmetic_op(vm, instr, [](auto a, auto b) { return a - b; });
        }

        static bool mul(VM& vm, const Instruction& instr) {
            return arithmetic_op(vm, instr, [](auto a, auto b) { return a * b; });
        }

        static bool div(VM& vm, const Instruction& instr) {
            return arithmetic_op(vm, instr, [](auto a, auto b) -> std::optional<std::uint64_t> {
                if (b == 0) {
                    return std::nullopt;
                }
                return a / b;
            });
        }

        static bool mod(VM& vm, const Instruction& instr) {
            return arithmetic_op(vm, instr, [](auto a, auto b) -> std::optional<std::uint64_t> {
                if (b == 0) {
                    return std::nullopt;
                }
                return a % b;
            });
        }

        static bool compare_op(VM& vm, const Instruction& instr, auto&& cb) {
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

        static bool cmp(VM& vm, const Instruction& instr) {
            return compare_op(vm, instr, [](auto a, auto b) {
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
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            if (arg.nop()) {
                return true;
            }
            auto from = arg.from();
            auto to = arg.to();
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

        static bool load_static(VM& vm, const Instruction& instr, const std::vector<Value>& static_data) {
            auto indexOfStatic = instr.arg();
            if (static_data.size() <= indexOfStatic) {
                return false;
            }
            vm.registers[0] = static_data[indexOfStatic];
            return true;
        }

        static bool push(VM& vm, const Instruction& instr) {
            auto arg = instr.arg();
            if (arg > 0xf) {
                return false;
            }
            vm.stack.push_back(vm.registers[arg]);
            return true;
        }

        static bool pop(VM& vm, const Instruction& instr) {
            if (vm.stack.empty()) {
                return false;
            }
            auto arg = instr.arg();
            if (arg > 0xf) {
                return false;
            }
            vm.registers[arg] = std::move(vm.stack.back());
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

        static bool load_immediate(VM& vm, const Instruction& instr) {
            vm.registers[0] = Value(instr.arg());
            return true;
        }

        static bool jump(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program) {
            auto arg = instr.arg();
            if (arg >= program.size()) {
                return false;
            }
            pc = arg;
            return true;
        }

        static bool jump_if(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program, auto&& cond) {
            auto arg = instr.arg();
            if (arg >= program.size()) {
                return false;
            }
            auto v = vm.registers[0].as_uint64();
            if (!v) {
                return false;
            }
            if (cond(*v)) {
                pc = arg;
                return true;
            }
            pc++;
            return true;
        }

        static bool jump_if_eq(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program) {
            return jump_if(vm, instr, pc, program, [](auto v) { return v == 0; });
        }

        static bool jump_if_not_eq(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program) {
            return jump_if(vm, instr, pc, program, [](auto v) { return v != 0; });
        }

        static bool read_bits_internal(VM& vm, const Instruction& instr, futils::binary::reader& r, size_t& read_bit_offset,
                                       size_t fraction_bit, size_t byte_size) {
            std::string result;
            if (fraction_bit != 0) {
                futils::byte data = 0;
                for (auto i = 0; i < fraction_bit; i++) {
                    if (r.empty()) {
                        return false;
                    }
                    data |= std::uint8_t(*ByteToBit{r.top()}[read_bit_offset]) << i;
                    read_bit_offset++;
                    if (read_bit_offset == 8) {
                        read_bit_offset = 0;
                        r.offset(1);
                    }
                }
                result.push_back(data);
            }
            if (read_bit_offset == 0) {  // fast pass
                auto offset = result.size();
                result.resize(offset + byte_size);
                if (!r.read(futils::view::wvec(result).substr(offset))) {
                    return false;
                }
            }
            else {
                for (size_t i = 0; i < byte_size; i++) {
                    for (size_t i = 0; i < 8; i++) {
                        if (r.empty()) {
                            return false;
                        }
                        result.push_back(std::uint8_t(*ByteToBit{r.top()}[read_bit_offset]));
                        read_bit_offset++;
                        if (read_bit_offset == 8) {
                            read_bit_offset = 0;
                            r.offset(1);
                        }
                    }
                }
            }
            vm.registers[0] = Value(std::move(result));
            return true;
        }

        static bool read_bits(VM& vm, const Instruction& instr, futils::binary::reader& r, size_t& read_bit_offset) {
            auto size = vm.registers[0].as_uint64();
            if (!size) {
                return false;
            }
            auto fraction_bit = *size & 0x7;
            auto byte_size = *size >> 3;
            return read_bits_internal(vm, instr, r, read_bit_offset, fraction_bit, byte_size);
        }

        static bool read_bytes(VM& vm, const Instruction& instr, futils::binary::reader& r, size_t& read_bit_offset) {
            auto size = vm.registers[0].as_uint64();
            if (!size) {
                return false;
            }
            return read_bits_internal(vm, instr, r, read_bit_offset, 0, *size);
        }

        static bool peek_bits(VM& vm, const Instruction& instr, futils::binary::reader& r, size_t& read_bit_offset) {
            auto cur_offset = r.offset();
            auto cur_read_bit_offset = read_bit_offset;
            auto res = read_bits(vm, instr, r, read_bit_offset);
            r.reset(cur_offset);
            read_bit_offset = cur_read_bit_offset;
            return res;
        }

        static bool peek_bytes(VM& vm, const Instruction& instr, futils::binary::reader& r, size_t& read_bit_offset) {
            auto cur_offset = r.offset();
            auto cur_read_bit_offset = read_bit_offset;
            auto res = read_bytes(vm, instr, r, read_bit_offset);
            r.reset(cur_offset);
            read_bit_offset = cur_read_bit_offset;
            return res;
        }

        static bool bytes_to_int(VM& vm, const Instruction& instr) {
            auto bytes = vm.registers[0].as_bytes();
            if (!bytes) {
                return false;
            }
            auto require_size = instr.arg();
            if (bytes->size() != require_size) {
                return false;
            }
            std::uint64_t result = 0;
            if (vm.endian == ast::Endian::big) {
                for (auto& byte : *bytes) {
                    result = (result << 8) | byte;
                }
            }
            else {
                std::reverse_iterator it{bytes->end()}, end{bytes->begin()};
                for (; it != end; it++) {
                    result = (result << 8) | *it;
                }
            }
            vm.registers[0] = Value(result);
            return true;
        }

        static bool set_endian(VM& vm, const Instruction& instr) {
            auto arg = instr.arg();
            if (arg == 0) {
                vm.endian = ast::Endian::big;
            }
            else if (arg == 1) {
                vm.endian = ast::Endian::little;
            }
            else {
                return false;
            }
            return true;
        }

        static bool next_func(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program, const std::vector<Value>& static_data) {
            if (pc + 2 >= program.size()) {
                return false;
            }
            if (program[pc + 1].op() != Op::FUNC_NAME) {
                return false;
            }
            auto index = program[pc + 1].arg();
            if (static_data.size() <= index) {
                return false;
            }
            auto name = static_data[index].as_bytes();
            if (!name) {
                return false;
            }
            auto key = std::string(name->as_char(), name->size());
            vm.functions[key] = pc + 2;
            vm.inverse_functions[pc + 2] = std::move(key);
            auto ptr = instr.arg();
            if (ptr > program.size()) {
                return false;
            }
            pc = ptr;
            return true;
        }

        static bool make_object(VM& vm, const Instruction& instr) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto to = arg.to();
            auto obj = std::make_shared<std::vector<Var>>();
            vm.registers[to] = Value(std::move(obj));
            return true;
        }

        static bool set_field(VM& vm, const Instruction& instr) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto obj = vm.registers[arg.to()].as_vars();
            if (!obj) {
                return false;
            }
            auto index = vm.registers[arg.index()].as_uint64();
            if (!index) {
                return false;
            }
            auto value = vm.registers[arg.from()];
            if (obj->size() <= *index) {
                obj->resize(*index + 1);
            }
            (*obj)[*index].value(std::move(value));
            return true;
        }

        static bool set_field_label(VM& vm, const Instruction& instr, const std::vector<Value>& static_data) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto obj = vm.registers[arg.to()].as_vars();
            if (!obj) {
                return false;
            }
            auto index = vm.registers[arg.index()].as_uint64();
            if (!index) {
                return false;
            }
            auto labelIndex = vm.registers[arg.from()].as_uint64();
            if (!labelIndex) {
                return false;
            }
            if (static_data.size() <= *labelIndex) {
                return false;
            }
            if (obj->size() <= *index) {
                obj->resize(*index + 1);
            }
            auto bytes = static_data[*labelIndex].as_bytes();
            if (!bytes) {
                return false;
            }
            (*obj)[*index].label(std::string(bytes->as_char(), bytes->size()));
            return true;
        }

        static bool get_field(VM& vm, const Instruction& instr) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto obj = vm.registers[arg.from()].as_vars();
            if (!obj) {
                return false;
            }
            auto index = vm.registers[arg.index()].as_uint64();
            if (!index) {
                return false;
            }
            if (obj->size() <= *index) {
                return false;
            }
            vm.registers[arg.to()] = (*obj)[*index].value();
            return true;
        }

        static bool func_end(VM& vm, size_t& pc, const std::vector<Instruction>& program) {
            if (vm.call_stack.empty()) {
                pc = program.size();
                return true;
            }
            auto ret = vm.call_stack.back();
            vm.call_stack.pop_back();
            vm.stack.resize(ret.ret_stack_size);
            pc = ret.ret_pc;
            return true;
        }

        static bool call(VM& vm, const Instruction& instr, size_t& pc) {
            auto arg = instr.arg();
            if (arg > 0xf) {
                return false;
            }
            auto next_pc = pc + 1;
            auto ptr = vm.registers[arg].as_uint64();
            if (!ptr) {
                return false;
            }
            vm.call_stack.push_back({size_t(*ptr), next_pc, vm.stack.size()});  // set clean up point
            pc = *ptr;
            return true;
        }

        static bool error(VM& vm, const Instruction& instr, size_t& pc, const std::vector<Instruction>& program) {
            auto data = vm.registers[0].as_bytes();
            if (!data) {
                return false;
            }
            vm.error_message = std::string(data->as_char(), data->size());
            for (auto it = vm.call_stack.rbegin(); it != vm.call_stack.rend(); it++) {
                auto found = vm.inverse_functions.find(it->call_target);
                if (found != vm.inverse_functions.end()) {
                    vm.error_message += " in function " + found->second;
                }
                else {
                    vm.error_message += " in function ???";
                }
            }
            pc = program.size();  // terminate the program
            return true;
        }

        static bool increment(VM& vm, const Instruction& instr) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto to = arg.to();
            auto a = vm.registers[to].as_uint64();
            if (!a) {
                return false;
            }
            vm.registers[to] = Value(*a + 1);
            return true;
        }

        static bool make_array(VM& vm, const Instruction& instr) {
            auto arg = TransferArg(instr.arg());
            if (!arg.valid()) {
                return false;
            }
            auto to = arg.to();
            auto size = vm.registers[arg.from()].as_uint64();
            if (!size) {
                return false;
            }
            vm.registers[to] = Value(std::make_shared<std::vector<Value>>(*size));
            return true;
        }
    };

    void VM::execute_internal(const Code& code, size_t& pc) {
        auto& program = code.instructions;
        auto& static_data = code.static_data;
        futils::binary::reader r{input};
        size_t read_bit_offset = 0;
        while (pc < program.size()) {
            if (inject) {
                inject(*this, program[pc], pc);
                if (pc >= program.size()) {
                    break;
                }
            }
            const auto& instr = program[pc];
            switch (instr.op()) {
                case Op::NOP:
                case Op::FUNC_NAME:
                    break;

#define exec_op(code, op, ...)                                 \
    case code: {                                               \
        if (!VMHelper::op(*this __VA_OPT__(, ) __VA_ARGS__)) { \
            error_message = #code " failed";                   \
            return;                                            \
        }                                                      \
        break;                                                 \
    }

#define change_pc_op(code, op, ...)                            \
    case code: {                                               \
        auto pre_pc = pc;                                      \
        if (!VMHelper::op(*this __VA_OPT__(, ) __VA_ARGS__)) { \
            error_message = #code " failed";                   \
        }                                                      \
        if (pc == pre_pc) {                                    \
            error_message = #code " is hung up";               \
            return;                                            \
        }                                                      \
        continue;                                              \
    }

                    change_pc_op(Op::FUNC_END, func_end, pc, program);
                    exec_op(Op::ADD, add, instr);
                    exec_op(Op::SUB, sub, instr);
                    exec_op(Op::MUL, mul, instr);
                    exec_op(Op::DIV, div, instr);
                    exec_op(Op::MOD, mod, instr);

                    exec_op(Op::CMP, cmp, instr);

                    exec_op(Op::INC, increment, instr);

                    exec_op(Op::TRSF, transfer, instr);

                    exec_op(Op::INIT_VAR, init_var, instr);
                    exec_op(Op::LOAD_VARIABLE, load_var, instr);
                    exec_op(Op::STORE_VARIABLE, store_var, instr);
                    exec_op(Op::LOAD_STATIC, load_static, instr, static_data);

                    exec_op(Op::PUSH, push, instr);
                    exec_op(Op::POP, pop, instr);

                    exec_op(Op::GET_OFFSET, get_offset, r);
                    exec_op(Op::SET_OFFSET, set_offset, r);

                    exec_op(Op::LOAD_IMMEDIATE, load_immediate, instr);

                    change_pc_op(Op::JMP, jump, instr, pc, program);
                    change_pc_op(Op::JE, jump_if_eq, instr, pc, program);
                    change_pc_op(Op::JNE, jump_if_not_eq, instr, pc, program);

                    exec_op(Op::READ_BITS, read_bits, instr, r, read_bit_offset);
                    exec_op(Op::READ_BYTES, read_bytes, instr, r, read_bit_offset);
                    exec_op(Op::PEEK_BITS, peek_bits, instr, r, read_bit_offset);
                    exec_op(Op::PEEK_BYTES, peek_bytes, instr, r, read_bit_offset);
                    exec_op(Op::BYTES_TO_INT, bytes_to_int, instr);
                    exec_op(Op::SET_ENDIAN, set_endian, instr);

                    change_pc_op(Op::NEXT_FUNC, next_func, instr, pc, program, static_data);

                    exec_op(Op::MAKE_OBJECT, make_object, instr);
                    exec_op(Op::SET_FIELD, set_field, instr);
                    exec_op(Op::GET_FIELD, get_field, instr);
                    exec_op(Op::SET_FIELD_LABEL, set_field_label, instr, static_data);

                    change_pc_op(Op::CALL, call, instr, pc);

                    change_pc_op(Op::ERROR, error, instr, pc, program);

                    exec_op(Op::MAKE_ARRAY, make_array, instr);

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

    void print_value(futils::helper::IPushBacker<> pb, const Value& val) {
        if (auto pval = val.as_uint64()) {
            futils::number::to_string(pb, *pval);
        }
        else if (auto pval = val.as_bytes()) {
            pb.push_back('"');
            for (auto c : *pval) {
                if (futils::number::is_ascii_non_control(c)) {
                    if (c == '"' || c == '\\') {
                        pb.push_back('\\');
                    }
                    pb.push_back(c);
                    continue;
                }
                pb.push_back('\\');
                pb.push_back('x');
                futils::number::insert_space(pb, 2, c, 16, '0');
                futils::number::to_string(pb, c, 16);
            }
            pb.push_back('"');
        }
        else if (auto pval = val.as_array()) {
            pb.push_back('[');
            bool first = true;
            for (const auto& v : *pval) {
                if (!first) {
                    futils::strutil::append(pb, ", ");
                }
                print_value(pb, v);
                first = false;
            }
            pb.push_back(']');
        }
        else if (auto pval = val.as_vars()) {
            pb.push_back('{');
            bool first = true;
            for (const auto& v : *pval) {
                if (!first) {
                    futils::strutil::append(pb, ", ");
                }
                pb.push_back('"');
                futils::strutil::append(pb, v.label());
                pb.push_back('"');
                pb.push_back(':');
                print_value(pb, v.value());
                first = false;
            }
            pb.push_back('}');
        }
    }
}  // namespace brgen::vm
