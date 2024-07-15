/*license*/
#pragma once

#include "interpret.h"
#include "vm2.h"

namespace brgen::vm2 {
    void VM2::reset(futils::view::rvec inst, futils::view::wvec memory, size_t stack_size) {
        instructions.reset_buffer(inst);
        full_memory = memory;
        this->stack_size = stack_size;
        get_register(Register::SP) = full_memory.size();
        get_register(Register::TRAP) = 0;
        get_register(Register::TRAP_REASON) = 0;
        get_register(Register::BP) = full_memory.size();
    }

    struct VM2Helper {
        static void load_integer(futils::view::rvec memory, std::uint64_t addr, size_t size, std::uint64_t& value) {
            value = 0;
            for (size_t i = 0; i < size; i++) {
                value |= std::uint64_t(memory[addr + i]) << (i * 8);
            }
        }

        static void store_integer(futils::view::wvec memory, std::uint64_t addr, size_t size, std::uint64_t value) {
            for (size_t i = 0; i < size; i++) {
                memory[addr + i] = std::uint8_t(value >> (i * 8));
            }
        }
        static bool load_memory(VM2& vm, const LoadMemory& mem) {
            auto from_addr = vm.get_register(mem.from);
            if (from_addr >= vm.full_memory.size()) {
                vm.set_trap(TrapNumber::INVALID_MEMORY_ACCESS, from_addr);
                return false;
            }
            if (mem.size > 8) {
                vm.set_trap(TrapNumber::LARGE_SIZE, mem.size);
                return false;
            }
            if (from_addr + mem.size >= vm.full_memory.size()) {
                vm.set_trap(TrapNumber::INVALID_MEMORY_ACCESS, from_addr + mem.size);
                return false;
            }
            load_integer(vm.full_memory, from_addr, mem.size, vm.get_register(mem.to));
            return true;
        }

        static bool store_memory(VM2& vm, const StoreMemory& mem) {
            auto to_addr = vm.get_register(mem.to);
            if (to_addr >= vm.full_memory.size()) {
                vm.set_trap(TrapNumber::INVALID_MEMORY_ACCESS, to_addr);
                return false;
            }
            if (mem.size > 8) {
                vm.set_trap(TrapNumber::LARGE_SIZE, mem.size);
                return false;
            }
            if (to_addr + mem.size >= vm.full_memory.size()) {
                vm.set_trap(TrapNumber::INVALID_MEMORY_ACCESS, vm.full_memory.size());
                return false;
            }
            store_integer(vm.full_memory, to_addr, mem.size, vm.get_register(mem.from));
            return true;
        }

        static bool jump(VM2& vm, Register target) {
            auto target_pc = vm.get_register(target);
            auto full_size = vm.instructions.read().size() + vm.instructions.remain().size();
            if (target_pc >= full_size) {
                vm.set_trap(TrapNumber::INVALID_JUMP, target_pc);
                return false;
            }
            vm.instructions.reset(target_pc);
            return true;
        }

        static bool push(VM2& vm, std::uint64_t value) {
            auto stack_top_addr = vm.get_register(Register::SP);
            if (vm.full_memory.size() - stack_top_addr >= vm.stack_size + 8) {
                vm.set_trap(TrapNumber::STACK_OVERFLOW, stack_top_addr);
                return false;
            }
            stack_top_addr -= 8;
            store_integer(vm.full_memory, stack_top_addr, 8, value);
            vm.get_register(Register::SP) = stack_top_addr;
            return true;
        }

        static bool pop(VM2& vm, std::uint64_t& value) {
            auto stack_top_addr = vm.get_register(Register::SP);
            auto stack_base_addr = vm.get_register(Register::BP);
            if (stack_top_addr + 8 > stack_base_addr) {
                vm.set_trap(TrapNumber::STACK_UNDERFLOW, stack_top_addr);
                return false;
            }
            load_integer(vm.full_memory, stack_top_addr, 8, value);
            vm.get_register(Register::SP) = stack_top_addr + 8;
            return true;
        }
    };

    void VM2::resume() {
        set_trap(TrapNumber::NO_TRAP, 0);
        while (get_trap() == TrapNumber::NO_TRAP) {
            step();
        }
    }

    std::pair<std::optional<Inst>, size_t> VM2::decode_inst() {
        size_t fail_offset = instructions.offset();
        set_trap(TrapNumber::NO_TRAP, 0);
        auto inst = enc::decode(instructions);
        if (!inst) {
            if (!instructions.empty()) {
                set_trap(TrapNumber::INVALID_INSTRUCTION, fail_offset);
            }
            else {
                set_trap(TrapNumber::END_OF_PROGRAM, 0);
            }
            return std::make_pair(std::nullopt, fail_offset);
        }
        return std::make_pair(inst, fail_offset);
    }

    std::optional<Inst> VM2::step() {
        auto [inst, fail_offset] = decode_inst();
        if (!inst) {
            return std::nullopt;
        }
        step_inst(*inst, fail_offset);
        return inst;
    }

    void VM2::step_inst(const Inst& inst_, size_t fail_offset) {
        get_register(Register::PC) = instructions.offset();
        auto inst = &inst_;
        switch (inst->op()) {
            case Op2::NOP:
                break;
            case Op2::TRSF: {
                auto t = *inst->transfer();
                if (t.to == Register::PC) {
                    set_trap(TrapNumber::INVALID_REGISTER_ACCESS, std::uint64_t(t.to));
                    return;
                }
                get_register(t.to) = get_register(t.from);
                break;
            }
            case Op2::LOAD_IMMEDIATE: {
                auto imm = *inst->load_immediate();
                get_register(imm.reg) = imm.value;
                break;
            }
            case Op2::LOAD_MEMORY: {
                auto mem = *inst->load_memory();
                if (!VM2Helper::load_memory(*this, mem)) {
                    return;
                }
                break;
            }
            case Op2::STORE_MEMORY: {
                auto mem = *inst->store_memory();
                if (!VM2Helper::store_memory(*this, mem)) {
                    return;
                }
                break;
            }
            case Op2::SYSCALL_IMMEDIATE: {
                auto imm = *inst->syscall();
                set_trap(TrapNumber::SYSCALL, std::uint64_t(imm));
                return;
            }
            case Op2::JMP: {
                auto target = *inst->jump();
                if (!VM2Helper::jump(*this, target.target)) {
                    return;
                }
                break;
            }
            case Op2::JMPIF: {
                auto jmp = *inst->jump_if();
                if (get_register(jmp.condition) != 0) {
                    if (!VM2Helper::jump(*this, jmp.target)) {
                        return;
                    }
                }
                break;
            }
            case Op2::PUSH: {
                auto push = *inst->push();
                if (!VM2Helper::push(*this, get_register(push.operand))) {
                    return;
                }
                break;
            }
            case Op2::PUSH_IMMEDIATE: {
                auto imm = *inst->push_immediate();
                if (!VM2Helper::push(*this, imm)) {
                    return;
                }
                break;
            }
            case Op2::POP: {
                auto pop = *inst->pop();
                if (!VM2Helper::pop(*this, get_register(pop.operand))) {
                    return;
                }
            }
            case Op2::CALL: {
                auto call = *inst->call();
                if (!VM2Helper::push(*this, get_register(Register::BP))) {  // save BP
                    return;
                }
                if (!VM2Helper::push(*this, get_register(Register::PC))) {  // save PC (return address)
                    return;
                }
                get_register(Register::BP) = get_register(Register::SP);  // set new BP
                if (!VM2Helper::jump(*this, call.target)) {               // jump to target
                    return;
                }
                break;
            }
            case Op2::RET: {
                get_register(Register::SP) = get_register(Register::BP);   // restore SP
                if (!VM2Helper::pop(*this, get_register(Register::PC))) {  // restore PC
                    return;
                }
                if (!VM2Helper::pop(*this, get_register(Register::BP))) {  // restore BP
                    return;
                }
                if (!VM2Helper::jump(*this, Register::PC)) {  // jump to return address
                    return;
                }
                break;
            }
            case Op2::INC:
            case Op2::DEC:
            case Op2::NEG:
            case Op2::NOT: {
                auto unary = *inst->unary_operator();
                switch (inst->op()) {
                    case Op2::INC:
                        get_register(unary.operand)++;
                        break;
                    case Op2::DEC:
                        get_register(unary.operand)--;
                        break;
                    case Op2::NEG:
                        get_register(unary.operand) = ~get_register(unary.operand) + 1;
                        break;
                    case Op2::NOT:
                        get_register(unary.operand) = ~get_register(unary.operand);
                        break;
                    default:
                        set_trap(TrapNumber::INVALID_INSTRUCTION, fail_offset);
                        return;
                }
            }
            case Op2::ADD:
            case Op2::SUB:
            case Op2::MUL:
            case Op2::DIV:
            case Op2::MOD:
            case Op2::AND:
            case Op2::OR:
            case Op2::XOR:
            case Op2::SHL:
            case Op2::SHR: {
                auto bin = *inst->binary_operator();
                switch (inst->op()) {
                    case Op2::ADD:
                        get_register(bin.result) = get_register(bin.left) + get_register(bin.right);
                        break;
                    case Op2::SUB:
                        get_register(bin.result) = get_register(bin.left) - get_register(bin.right);
                        break;
                    case Op2::MUL:
                        get_register(bin.result) = get_register(bin.left) * get_register(bin.right);
                        break;
                    case Op2::DIV:
                        if (get_register(bin.right) == 0) {
                            set_trap(TrapNumber::DIVISION_BY_ZERO, fail_offset);
                            return;
                        }
                        get_register(bin.result) = get_register(bin.left) / get_register(bin.right);
                        break;
                    case Op2::MOD:
                        if (get_register(bin.right) == 0) {
                            set_trap(TrapNumber::DIVISION_BY_ZERO, fail_offset);
                            return;
                        }
                        get_register(bin.result) = get_register(bin.left) % get_register(bin.right);
                        break;
                    case Op2::AND:
                        get_register(bin.result) = get_register(bin.left) & get_register(bin.right);
                        break;
                    case Op2::OR:
                        get_register(bin.result) = get_register(bin.left) | get_register(bin.right);
                        break;
                    case Op2::XOR:
                        get_register(bin.result) = get_register(bin.left) ^ get_register(bin.right);
                        break;
                    case Op2::SHL:
                        get_register(bin.result) = get_register(bin.left) << get_register(bin.right);
                        break;
                    case Op2::SHR:
                        get_register(bin.result) = get_register(bin.left) >> get_register(bin.right);
                        break;
                    default:
                        set_trap(TrapNumber::INVALID_INSTRUCTION, fail_offset);
                        return;
                }
                break;
            }
            default:
                set_trap(TrapNumber::INVALID_INSTRUCTION, fail_offset);
                return;
        }
    }

    bool VM2::handle_syscall(futils::binary::reader* input, futils::binary::writer* output) {
        if (get_trap() != TrapNumber::SYSCALL) {
            return false;
        }
        switch (SyscallNumber(get_trap_reason())) {
            case SyscallNumber::ALLOCATE: {
                auto size = get_register(Register::R1);
                if (allocation_map.size() == 0) {  // initialize entry
                    allocation_map.push_back(AllocationEntry{0, full_memory.size() - stack_size, false});
                }
                for (auto i = allocation_map.begin(); i != allocation_map.end(); i++) {
                    if (!i->used && i->size >= size) {
                        // split entry
                        if (i->size > size) {
                            allocation_map.insert(std::next(i), AllocationEntry{i->address + size, i->size - size, false});
                            i->size = size;
                        }
                        i->used = true;
                        set_syscall_return(i->address);
                        return true;
                    }
                }
                set_trap(TrapNumber::OUT_OF_MEMORY, size);
                return false;
            }
            case SyscallNumber::READ_IN:
            case SyscallNumber::PEEK_IN: {
                if (!input) {
                    set_trap(TrapNumber::INVALID_SYSCALL, get_trap_reason());
                    return false;
                }
                auto read_ptr = get_register(Register::R1);
                auto read_size = get_register(Register::R2);
                if (full_memory.size() < read_ptr + read_size) {
                    set_trap(TrapNumber::INVALID_MEMORY_ACCESS, read_ptr + read_size);
                    return false;
                }
                auto buffer = full_memory.substr(read_ptr, read_size);
                if (!input->read(buffer)) {
                    set_trap(TrapNumber::INVALID_SYSCALL, get_trap_reason());
                    return false;
                }
                return true;
            }
            default: {
                set_trap(TrapNumber::INVALID_SYSCALL, get_trap_reason());
                return false;
            }
        }
    }
}  // namespace brgen::vm2
