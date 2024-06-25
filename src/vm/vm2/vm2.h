/*license*/
#pragma once
#include "vm_enum.h"
#include <optional>
#include <view/iovec.h>
#include <binary/number.h>

namespace brgen::vm {

    struct TwoRegisters {
        Register from;
        Register to;
    };
    struct RegisterAndValue {
        Register reg;
        std::uint64_t value;
    };

    struct ThreeRegisters {
        Op2 op;
        Register operand1;
        Register operand2;
        Register result;
    };

    struct SingleRegister {
        Op2 op;
        Register operand;
    };

    struct Target {
        Register target;
    };

    struct TargetWithCondition {
        Register target;
        Register condition;
    };

    using Transfer = TwoRegisters;
    using LoadMemory = TwoRegisters;
    using StoreMemory = TwoRegisters;
    using LoadImmediate = RegisterAndValue;

    struct Inst {
       private:
        Op2 op_ = Op2::NOP;
        std::uint64_t arg1_ = 0;
        std::uint64_t arg2_ = 0;
        std::uint64_t arg3_ = 0;

       public:
        constexpr Inst(Op2 op, std::uint64_t arg1 = 0, std::uint64_t arg2 = 0, std::uint64_t arg3 = 0)
            : op_(op), arg1_(arg1), arg2_(arg2), arg3_(arg3) {}

        constexpr Inst(const Inst&) = default;
        constexpr Inst(Inst&&) = default;

        constexpr Op2 op() const {
            return op_;
        }

        constexpr std::optional<Transfer> transfer() const {
            if (op_ == Op2::TRSF) {
                return Transfer{static_cast<Register>(arg1_), static_cast<Register>(arg2_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<LoadMemory> load_memory() const {
            if (op_ == Op2::LOAD_MEMORY) {
                return LoadMemory{static_cast<Register>(arg1_), static_cast<Register>(arg2_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<StoreMemory> store_memory() const {
            if (op_ == Op2::STORE_MEMORY) {
                return StoreMemory{static_cast<Register>(arg1_), static_cast<Register>(arg2_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<LoadImmediate> load_immediate() const {
            if (op_ == Op2::LOAD_IMMEDIATE) {
                return LoadImmediate{static_cast<Register>(arg1_), arg2_};
            }
            return std::nullopt;
        }

        constexpr std::optional<ThreeRegisters> binary_operator() const {
            if (op_ == Op2::ADD || op_ == Op2::SUB || op_ == Op2::MUL || op_ == Op2::DIV || op_ == Op2::MOD ||
                op_ == Op2::AND || op_ == Op2::OR || op_ == Op2::XOR || op_ == Op2::SHL || op_ == Op2::SHR ||
                op_ == Op2::EQ || op_ == Op2::NE || op_ == Op2::LT || op_ == Op2::LE) {
                return ThreeRegisters{op_, static_cast<Register>(arg1_), static_cast<Register>(arg2_), static_cast<Register>(arg3_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<SingleRegister> unary_operator() const {
            if (op_ == Op2::NOT || op_ == Op2::NEG || op_ == Op2::INC || op_ == Op2::DEC) {
                return SingleRegister{op_, static_cast<Register>(arg1_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<Target> jump() const {
            if (op_ == Op2::JMP) {
                return Target{static_cast<Register>(arg1_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<Target> call() const {
            if (op_ == Op2::CALL) {
                return Target{static_cast<Register>(arg1_)};
            }
            return std::nullopt;
        }

        constexpr bool ret() const {
            return op_ == Op2::RET;
        }

        constexpr std::optional<TargetWithCondition> jump_if() const {
            if (op_ == Op2::JMPIF) {
                return TargetWithCondition{static_cast<Register>(arg1_), static_cast<Register>(arg2_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<SingleRegister> push() const {
            if (op_ == Op2::PUSH) {
                return SingleRegister{op_, static_cast<Register>(arg1_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<SingleRegister> pop() const {
            if (op_ == Op2::POP) {
                return SingleRegister{op_, static_cast<Register>(arg1_)};
            }
            return std::nullopt;
        }

        constexpr std::optional<std::uint64_t> push_immediate() const {
            if (op_ == Op2::PUSH_IMMEDIATE) {
                return arg1_;
            }
            return std::nullopt;
        }
    };

    namespace inst {
        constexpr auto nop() {
            return Inst(Op2::NOP);
        }

        constexpr auto transfer(Register from, Register to) {
            return Inst(Op2::TRSF, static_cast<std::uint64_t>(from), static_cast<std::uint64_t>(to));
        }

        constexpr auto load_memory(Register from, Register to) {
            return Inst(Op2::LOAD_MEMORY, static_cast<std::uint64_t>(from), static_cast<std::uint64_t>(to));
        }

        constexpr auto store_memory(Register from, Register to) {
            return Inst(Op2::STORE_MEMORY, static_cast<std::uint64_t>(from), static_cast<std::uint64_t>(to));
        }

        constexpr auto load_immediate(Register to, std::uint64_t value) {
            return Inst(Op2::LOAD_IMMEDIATE, static_cast<std::uint64_t>(to), value);
        }

        // binary operators

        constexpr auto binary_operator(Op2 op, Register operand1, Register operand2, Register result) {
            return Inst(op, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto add(Register operand1, Register operand2, Register result) {
            return Inst(Op2::ADD, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto sub(Register operand1, Register operand2, Register result) {
            return Inst(Op2::SUB, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto mul(Register operand1, Register operand2, Register result) {
            return Inst(Op2::MUL, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto div(Register operand1, Register operand2, Register result) {
            return Inst(Op2::DIV, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto mod(Register operand1, Register operand2, Register result) {
            return Inst(Op2::MOD, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto and_(Register operand1, Register operand2, Register result) {
            return Inst(Op2::AND, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto or_(Register operand1, Register operand2, Register result) {
            return Inst(Op2::OR, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto xor_(Register operand1, Register operand2, Register result) {
            return Inst(Op2::XOR, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto shift_left(Register operand1, Register operand2, Register result) {
            return Inst(Op2::SHL, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto shift_right(Register operand1, Register operand2, Register result) {
            return Inst(Op2::SHR, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto eq(Register operand1, Register operand2, Register result) {
            return Inst(Op2::EQ, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto ne(Register operand1, Register operand2, Register result) {
            return Inst(Op2::NE, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto lt(Register operand1, Register operand2, Register result) {
            return Inst(Op2::LT, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto le(Register operand1, Register operand2, Register result) {
            return Inst(Op2::LE, static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(result));
        }

        constexpr auto gt(Register operand1, Register operand2, Register result) {
            return Inst(Op2::LT, static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(result));
        }

        constexpr auto ge(Register operand1, Register operand2, Register result) {
            return Inst(Op2::LE, static_cast<std::uint64_t>(operand2), static_cast<std::uint64_t>(operand1), static_cast<std::uint64_t>(result));
        }

        // unary operators

        constexpr auto unary_operator(Op2 op, Register operand) {
            return Inst(op, static_cast<std::uint64_t>(operand));
        }

        constexpr auto not_(Register operand, Register result) {
            return Inst(Op2::NOT, static_cast<std::uint64_t>(operand), static_cast<std::uint64_t>(result));
        }

        constexpr auto neg(Register operand, Register result) {
            return Inst(Op2::NEG, static_cast<std::uint64_t>(operand), static_cast<std::uint64_t>(result));
        }

        constexpr auto inc(Register operand, Register result) {
            return Inst(Op2::INC, static_cast<std::uint64_t>(operand), static_cast<std::uint64_t>(result));
        }

        constexpr auto dec(Register operand, Register result) {
            return Inst(Op2::DEC, static_cast<std::uint64_t>(operand), static_cast<std::uint64_t>(result));
        }

        constexpr auto jump(Register to) {
            return Inst(Op2::JMP, static_cast<std::uint64_t>(to));
        }

        constexpr auto jump_if(Register to, Register condition) {
            return Inst(Op2::JMPIF, static_cast<std::uint64_t>(to), static_cast<std::uint64_t>(condition));
        }

        constexpr auto call(Register to) {
            return Inst(Op2::CALL, static_cast<std::uint64_t>(to));
        }

        constexpr auto ret() {
            return Inst(Op2::RET);
        }

        constexpr auto push(Register reg) {
            return Inst(Op2::PUSH, static_cast<std::uint64_t>(reg));
        }

        constexpr auto pop(Register reg) {
            return Inst(Op2::POP, static_cast<std::uint64_t>(reg));
        }

        constexpr auto push_immediate(std::uint64_t value) {
            return Inst(Op2::PUSH_IMMEDIATE, value);
        }

    }  // namespace inst

    namespace enc {
        inline auto encode(futils::binary::writer& w, const Inst& inst) {
            brgen::vm::Op2Inst op2_inst;
            op2_inst.op = inst.op();
            if (auto tf = inst.transfer()) {
                op2_inst.to(tf->to);
                op2_inst.from(tf->from);
            }
            else if (auto lm = inst.load_memory()) {
                op2_inst.to(lm->to);
                op2_inst.from(lm->from);
            }
            else if (auto sm = inst.store_memory()) {
                op2_inst.to(sm->to);
                op2_inst.from(sm->from);
            }
            else if (auto li = inst.load_immediate()) {
                op2_inst.to(li->reg);
                op2_inst.immediate(li->value);
            }
            else if (auto bo = inst.binary_operator()) {
                op2_inst.operand1(bo->operand1);
                op2_inst.operand2(bo->operand2);
                op2_inst.result(bo->result);
            }
            else if (auto uo = inst.unary_operator()) {
                op2_inst.operand(uo->operand);
            }
            else if (auto jmp = inst.jump()) {
                op2_inst.to(jmp->target);
            }
            else if (auto call = inst.call()) {
                op2_inst.to(call->target);
            }
            else if (inst.ret()) {
                op2_inst.to(Register::R0);
            }
            else if (auto jif = inst.jump_if()) {
                op2_inst.to(jif->target);
                op2_inst.condition(jif->condition);
            }
            else if (auto push = inst.push()) {
                op2_inst.to(push->operand);
            }
            else if (auto pop = inst.pop()) {
                op2_inst.to(pop->operand);
            }
            else if (auto pi = inst.push_immediate()) {
                op2_inst.immediate(pi.value());
            }
            return op2_inst.encode(w);
        }

        inline std::optional<Inst> decode(futils::binary::reader& r) {
            brgen::vm::Op2Inst op2_inst;
            if (!op2_inst.decode(r)) {
                return std::nullopt;
            }
            auto op = op2_inst.op;
            switch (op) {
                case Op2::NOP:
                    return inst::nop();
                case Op2::TRSF:
                    return inst::transfer(*op2_inst.from(), *op2_inst.to());
                case Op2::LOAD_MEMORY:
                    return inst::load_memory(*op2_inst.from(), *op2_inst.to());
                case Op2::STORE_MEMORY:
                    return inst::store_memory(*op2_inst.from(), *op2_inst.to());
                case Op2::LOAD_IMMEDIATE:
                    return inst::load_immediate(*op2_inst.to(), *op2_inst.immediate());
                case Op2::ADD:
                case Op2::SUB:
                case Op2::MUL:
                case Op2::DIV:
                case Op2::MOD:
                case Op2::AND:
                case Op2::OR:
                case Op2::XOR:
                case Op2::SHL:
                case Op2::SHR:
                case Op2::EQ:
                case Op2::NE:
                case Op2::LT:
                case Op2::LE:
                    return inst::binary_operator(op, *op2_inst.operand1(), *op2_inst.operand2(), *op2_inst.result());
                case Op2::NOT:
                case Op2::NEG:
                case Op2::INC:
                case Op2::DEC:
                    return inst::unary_operator(op, *op2_inst.operand());
                case Op2::JMP:
                    return inst::jump(*op2_inst.to());
                case Op2::CALL:
                    return inst::call(*op2_inst.to());
                case Op2::RET:
                    return inst::ret();
                case Op2::JMPIF:
                    return inst::jump_if(*op2_inst.to(), *op2_inst.condition());
                case Op2::PUSH:
                    return inst::push(*op2_inst.from());
                case Op2::POP:
                    return inst::pop(*op2_inst.to());
                case Op2::PUSH_IMMEDIATE:
                    return inst::push_immediate(*op2_inst.immediate());
                default:
                    return std::nullopt;
            }
        }
    }  // namespace enc

    struct MemoryLayout {
        std::uint64_t stack_size;
        std::uint64_t instruction_size;
        std::uint64_t instruction_start;
    };

    struct VM2 {
       private:
        futils::view::wvec full_memory;
        futils::view::wvec stack;
        futils::binary::reader instructions{futils::view::rvec{}};

       public:
    };
}  // namespace brgen::vm
