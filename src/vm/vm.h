/*license*/
#pragma once
#include "vm_enum.h"
#include <variant>
#include <view/iovec.h>
#include <vector>
#include <string>
#include <binary/flags.h>
#include <unordered_map>
#include <memory>
#include <core/ast/node/ast_enum.h>
#include <helper/pushbacker.h>

namespace brgen::vm {
    struct Instruction {
       private:
        Op op_ = Op::NOP;
        std::uint64_t arg_ = 0;

       public:
        constexpr Instruction(Op op, std::uint64_t arg = 0)
            : op_(op), arg_(arg) {}

        constexpr Op op() const {
            return op_;
        }
        constexpr std::uint64_t arg() const {
            return arg_;
        }

        constexpr void arg(std::uint64_t arg) {
            arg_ = arg;
        }
    };

    struct Var;
    struct Value {
       private:
        std::variant<uint64_t, futils::view::rvec, std::string, std::shared_ptr<std::vector<Value>>,
                     std::shared_ptr<std::vector<Var>>>
            data;

       public:
        constexpr Value()
            : Value(0) {}
        constexpr Value(uint64_t val)
            : data(val) {}
        constexpr Value(futils::view::rvec val)
            : data(val) {}

        Value(std::shared_ptr<std::vector<Var>> val)
            : data(std::move(val)) {}

        Value(std::shared_ptr<std::vector<Value>> val)
            : data(std::move(val)) {}

        Value(std::string val)
            : data(std::move(val)) {}

        constexpr std::optional<std::uint64_t> as_uint64() const {
            if (auto pval = std::get_if<uint64_t>(&data)) {
                return *pval;
            }
            return std::nullopt;
        }

        // rvec or string
        constexpr std::optional<futils::view::rvec> as_bytes() const {
            if (auto pval = std::get_if<futils::view::rvec>(&data)) {
                return *pval;
            }
            if (auto pval = std::get_if<std::string>(&data)) {
                return futils::view::rvec(*pval);
            }
            return std::nullopt;
        }

        std::vector<Var>* as_vars() {
            if (auto p = std::get_if<std::shared_ptr<std::vector<Var>>>(&data)) {
                return p->get();
            }
            return nullptr;
        }

        const std::vector<Var>* as_vars() const {
            if (auto p = std::get_if<std::shared_ptr<std::vector<Var>>>(&data)) {
                return p->get();
            }
            return nullptr;
        }

        std::vector<Value>* as_array() {
            if (auto p = std::get_if<std::shared_ptr<std::vector<Value>>>(&data)) {
                return p->get();
            }
            return nullptr;
        }

        const std::vector<Value>* as_array() const {
            if (auto p = std::get_if<std::shared_ptr<std::vector<Value>>>(&data)) {
                return p->get();
            }
            return nullptr;
        }
    };

    struct Var {
       private:
        std::string label_;
        Value value_;

       public:
        Var() = default;

        Var(std::string&& label, Value&& value)
            : label_(std::move(label)), value_(std::move(value)) {}

        constexpr const std::string& label() const {
            return label_;
        }
        constexpr const Value& value() const {
            return value_;
        }

        void label(std::string&& label) {
            label_ = std::move(label);
        }

        void value(Value&& value) {
            value_ = std::move(value);
        }
    };

    struct Code {
        std::vector<Instruction> instructions;
        std::vector<Value> static_data;
    };

    struct TransferArg {
       private:
        futils::binary::flags_t<std::uint64_t, 52, 4, 4, 4> flags;
        bits_flag_alias_method(flags, 0, reserved);

       public:
        bits_flag_alias_method(flags, 1, index);
        bits_flag_alias_method(flags, 2, from);
        bits_flag_alias_method(flags, 3, to);

        constexpr TransferArg(uint8_t from, uint8_t to, uint8_t index = 0) {
            this->from(from);
            this->to(to);
            this->index(index);
        }

        constexpr TransferArg(std::uint64_t val)
            : flags(val) {}

        constexpr operator std::uint64_t() const {
            return flags.as_value();
        }

        constexpr bool valid() const {
            return reserved() == 0;
        }

        constexpr bool nop() const {
            return from() == to();
        }
    };

    struct ByteToBit {
       private:
        futils::binary::flags_t<futils::byte, 1, 1, 1, 1, 1, 1, 1, 1> flags;
        bits_flag_alias_method(flags, 0, b0);
        bits_flag_alias_method(flags, 1, b1);
        bits_flag_alias_method(flags, 2, b2);
        bits_flag_alias_method(flags, 3, b3);
        bits_flag_alias_method(flags, 4, b4);
        bits_flag_alias_method(flags, 5, b5);
        bits_flag_alias_method(flags, 6, b6);
        bits_flag_alias_method(flags, 7, b7);

       public:
        constexpr ByteToBit(futils::byte val)
            : flags(val) {}

        constexpr operator futils::byte() const {
            return flags.as_value();
        }

        constexpr std::optional<bool> operator[](std::size_t idx) const {
            switch (idx) {
                case 0:
                    return b0();
                case 1:
                    return b1();
                case 2:
                    return b2();
                case 3:
                    return b3();
                case 4:
                    return b4();
                case 5:
                    return b5();
                case 6:
                    return b6();
                case 7:
                    return b7();
                default:
                    return std::nullopt;
            }
        }

        constexpr bool set(std::size_t idx, bool val) {
            switch (idx) {
                case 0:
                    b0(val);
                    return true;
                case 1:
                    b1(val);
                    return true;
                case 2:
                    b2(val);
                    return true;
                case 3:
                    b3(val);
                    return true;
                case 4:
                    b4(val);
                    return true;
                case 5:
                    b5(val);
                    return true;
                case 6:
                    b6(val);
                    return true;
                case 7:
                    b7(val);
                    return true;
                default:
                    return false;
            }
        }
    };

    constexpr auto register_size = 16;
    constexpr auto this_register = 15;

    struct CallStack {
        size_t call_target;
        size_t ret_pc;
        size_t ret_stack_size;
    };

    struct VM {
       private:
        Value registers[register_size];
        std::vector<Value> stack;
        std::vector<CallStack> call_stack;
        std::string error_message;
        std::vector<Var> global_variables;
        friend struct VMHelper;

        futils::view::rvec input;
        std::string output;
        std::unordered_map<std::string, std::uint64_t> functions;
        std::unordered_map<std::uint64_t, std::string> inverse_functions;
        ast::Endian endian = ast::Endian::big;

        void execute_internal(const Code& code, size_t& pc);

        void (*inject)(VM& vm, const Instruction& instr, size_t& pc) = nullptr;

       public:
        VM() = default;

        void set_endian(ast::Endian endian) {
            this->endian = endian;
        }

        void set_inject(void (*inject)(VM& vm, const Instruction& instr, size_t& pc)) {
            this->inject = inject;
        }

        constexpr void set_input(futils::view::rvec input) {
            this->input = input;
        }

        constexpr futils::view::rvec get_input() const {
            return input;
        }

        constexpr futils::view::rvec get_output() const {
            return output;
        }
        void execute(const Code& code) {
            size_t pc = 0;
            execute_internal(code, pc);
        }

        void execute(const Instruction& instr) {
            Code code;
            code.instructions.push_back(instr);
            size_t pc = 0;
            execute_internal(code, pc);
        }

        void call(const Code& code, const std::string& name) {
            auto it = functions.find(name);
            if (it == functions.end()) {
                error_message = "function not found: " + name;
                return;
            }
            call_stack.push_back({size_t(it->second), code.instructions.size(), stack.size()});
            size_t pc = it->second;
            execute_internal(code, pc);
        }

        constexpr const std::string& error() const {
            return error_message;
        }

        Value get_register(size_t index) const {
            if (index < register_size) {
                return registers[index];
            }
            return {};
        }
    };

    void print_value(futils::helper::IPushBacker<> pb, const Value& val);

}  // namespace brgen::vm
