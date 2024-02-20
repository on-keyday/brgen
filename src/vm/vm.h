/*license*/
#pragma once
#include "vm_enum.h"
#include <variant>
#include <view/iovec.h>
#include <vector>

namespace brgen::vm {
    struct Instruction {
       private:
        Op op_ = Op::NOP;
        std::uint64_t arg_ = 0;

       public:
        constexpr Instruction(Op op, std::uint64_t arg)
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

    struct Value {
       private:
        std::variant<uint64_t, futils::view::rvec, std::string> data;

       public:
        constexpr Value(uint64_t val)
            : data(val) {}
        constexpr Value(futils::view::rvec val)
            : data(val) {}

        constexpr Value(std::string val)
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
    };

    struct Var {
       private:
        std::string label_;
        Value value_;

       public:
        constexpr Var(std::string&& label, Value&& value)
            : label_(std::move(label)), value_(std::move(value)) {}

        constexpr const std::string& label() const {
            return label_;
        }
        constexpr const Value& value() const {
            return value_;
        }

        constexpr void value(Value&& value) {
            value_ = std::move(value);
        }
    };

    struct VM {
       private:
        Value registers[16];
        std::vector<Value> stack;
        std::string error_message;
        std::vector<Var> variables;
        friend struct VMHelper;

        futils::view::rvec input;
        std::string output;

       public:
        constexpr void set_input(futils::view::rvec input) {
            this->input = input;
        }
        constexpr futils::view::rvec get_output() const {
            return output;
        }
        void execute(const std::vector<Instruction>& program, const std::vector<Value>& static_data);
    };

}  // namespace brgen::vm
