/*license*/
#pragma once
#include <jit/x64.h>
#include <jit/jit_memory.h>
#include "interpret.h"
#include <string>

namespace brgen::vm2 {

    auto map_register(Register reg) {
        switch (reg) {
            case Register::R0:
                return futils::jit::x64::Register::RAX;
            case Register::R1:
                return futils::jit::x64::Register::RCX;
            case Register::R2:
                return futils::jit::x64::Register::RDX;
            case Register::R3:
                return futils::jit::x64::Register::RBX;
            case Register::R4:
                return futils::jit::x64::Register::RSP;
            case Register::R5:
                return futils::jit::x64::Register::RBP;
            case Register::R6:
                return futils::jit::x64::Register::RSI;
            case Register::R7:
                return futils::jit::x64::Register::RDI;
            case Register::R8:
                return futils::jit::x64::Register::R8;
            case Register::R9:
                return futils::jit::x64::Register::R9;
            case Register::R10:
                return futils::jit::x64::Register::R10;
            case Register::R11:
                return futils::jit::x64::Register::R11;
            case Register::R12:
                return futils::jit::x64::Register::R12;
            case Register::R13:
                return futils::jit::x64::Register::R13;
            case Register::R14:
                return futils::jit::x64::Register::R14;
            case Register::OBJECT_POINTER:
                return futils::jit::x64::Register::R15;
            default:
                break;
        }
        return futils::jit::x64::Register::RAX;
    }

    enum RelocationType {
        SAVE_RIP_RBP_RSP,
    };

    struct RelocationEntry {
        RelocationType type;
        std::uint64_t rewrite_offset = 0;
        std::uint64_t end_offset = 0;
        std::uint64_t address_offset = 0;
    };

    constexpr auto rbp_saved = futils::jit::x64::Register::R9;
    constexpr auto rsp_saved = futils::jit::x64::Register::R10;

    void write_coroutine_prologue(futils::binary::writer& w, futils::view::wvec full_memory) {
        // function argument registers are RDI, RSI, RDX, RCX, R8, R9
        // this function is int (*)()
        // stack bottom layout (from bottom):
        // 0. next instruction pointer like coroutine
        // 1. next stack rbp like coroutine
        // 2. next stack rsp like coroutine
        // 3. original (system) rbp // at entry, this is the stack bottom
        // 4. original (system) rsp

        // save rbp and rsp
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RSP, rbp_saved);
        futils::jit::x64::emit_mov_reg_reg(w, futils::jit::x64::Register::RBP, rsp_saved);
        // change stack pointer to point to the bottom of the stack
        auto stack_bottom = std::uint64_t(full_memory.data() + full_memory.size());
        auto next_instruction_pointer = stack_bottom - 8 * 1;
        auto next_stack_rbp = stack_bottom - 8 * 2;
        auto next_stack_rsp = stack_bottom - 8 * 3;
        auto rbp_bottom = stack_bottom - 8 * 3;  // for coroutine instruction pointer, rbp, rsp
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, rbp_bottom);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, rbp_bottom);
        futils::jit::x64::emit_push_reg(w, rsp_saved);
        futils::jit::x64::emit_push_reg(w, rbp_saved);
        auto initial_stack_pointer = stack_bottom - 8 * 5;
        futils::binary::writer mem{full_memory};
        mem.reset(full_memory.size() - 8 * 3);
        futils::binary::write_num(mem, initial_stack_pointer, false);  // writing next rsp for first resume
        futils::binary::write_num(mem, rbp_bottom, false);             // writing next rbp for first resume
        // now we are going to jump to the suspended point
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, next_stack_rsp);
        futils::jit::x64::emit_mov_reg_mem(w, futils::jit::x64::Register::RSP, futils::jit::x64::Register::RSP);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, next_stack_rbp);
        futils::jit::x64::emit_mov_reg_mem(w, futils::jit::x64::Register::RBP, futils::jit::x64::Register::RBP);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, next_instruction_pointer);
        futils::jit::x64::emit_push_reg(w, futils::jit::x64::Register::RAX);
        futils::jit::x64::emit_ret(w);  // jump to the suspended point
    }

    void write_coroutine_epilogue(futils::binary::writer& w, futils::view::wvec full_memory) {
        auto initial_stack_pointer = std::uint64_t(full_memory.data() + full_memory.size() - 8 * 5);
        auto rbp_bottom = std::uint64_t(full_memory.data() + full_memory.size()) - 8 * 3;
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RSP, initial_stack_pointer);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RBP, rbp_bottom);
        futils::jit::x64::emit_pop_reg(w, rbp_saved);
        futils::jit::x64::emit_pop_reg(w, rsp_saved);
        futils::jit::x64::emit_mov_reg_reg(w, rbp_saved, futils::jit::x64::Register::RBP);
        futils::jit::x64::emit_mov_reg_reg(w, rsp_saved, futils::jit::x64::Register::RSP);
        futils::jit::x64::emit_ret(w);
    }

    void write_save_instruction_pointer_rbp_rsp(futils::binary::writer& w, futils::view::wvec full_memory, std::uint64_t instruction_pointer) {
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, instruction_pointer);
        auto next_instruction_pointer_saved = std::uint64_t(full_memory.data() + full_memory.size() - 8 * 1);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::R8, next_instruction_pointer_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::RAX, futils::jit::x64::Register::R8);
        auto next_stack_rbp_saved = std::uint64_t(full_memory.data() + full_memory.size() - 8 * 2);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, next_stack_rbp_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::RBP, futils::jit::x64::Register::RAX);
        auto next_stack_rsp_saved = std::uint64_t(full_memory.data() + full_memory.size() - 8 * 3);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, next_stack_rsp_saved);
        futils::jit::x64::emit_mov_mem_reg(w, futils::jit::x64::Register::RSP, futils::jit::x64::Register::RAX);
    }

    void write_initial_next_instruction_pointer(futils::view::wvec full_memory, std::uint64_t next_instruction_pointer) {
        futils::binary::writer mem{full_memory};
        mem.reset(full_memory.size() - 8 * 1);
        futils::binary::write_num(mem, next_instruction_pointer, false);
    }

    futils::jit::ExecutableMemory VM2::jit_compile() {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        write_coroutine_prologue(w, full_memory);
        std::vector<RelocationEntry> relocations;
        for (;;) {
            auto [inst, offset] = decode_inst();
            if (!inst) {
                break;
            }
            switch (inst->op()) {
                case Op2::NOP:
                case Op2::FUNC_ENTRY:
                    futils::jit::x64::emit_nop(w);
                    break;
                case Op2::LOAD_IMMEDIATE: {
                    auto imm = *inst->load_immediate();
                    auto to = map_register(imm.reg);
                    futils::jit::x64::emit_mov_reg_imm(w, to, imm.value);
                    break;
                }
                case Op2::TRSF: {
                    auto trsf = *inst->transfer();
                    auto from = map_register(trsf.from);
                    auto to = map_register(trsf.to);
                    futils::jit::x64::emit_mov_reg_reg(w, from, to);
                    break;
                }
                case Op2::LOAD_MEMORY: {
                    auto mem = *inst->load_memory();
                    auto from = map_register(mem.from);
                    auto to = map_register(mem.to);
                    futils::jit::x64::emit_mov_reg_mem(w, from, to);
                }
                case Op2::STORE_MEMORY: {
                    auto mem = *inst->store_memory();
                    auto from = map_register(mem.from);
                    auto to = map_register(mem.to);
                    futils::jit::x64::emit_mov_mem_reg(w, from, to);
                }
                case Op2::SYSCALL_IMMEDIATE: {
                    futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, std::uint64_t(TrapNumber::SYSCALL));
                    auto rewrite_offset = w.offset();
                    write_save_instruction_pointer_rbp_rsp(w, full_memory, 0);
                    auto end_offset = w.offset();
                    write_coroutine_epilogue(w, full_memory);
                    auto address_offset = w.offset();
                    relocations.push_back({SAVE_RIP_RBP_RSP, rewrite_offset, end_offset, address_offset});
                }
                default: {
                    break;
                }
            }
        }
        write_coroutine_epilogue(w, full_memory);
        auto editable = futils::jit::EditableMemory::allocate(w.written().size());
        if (!editable.valid()) {
            return {};
        }
        memcpy(editable.get_memory().data(), w.written().data(), w.written().size());
        write_initial_next_instruction_pointer(full_memory, std::uint64_t(editable.get_memory().data()));
        for (auto& relocation : relocations) {
            switch (relocation.type) {
                case SAVE_RIP_RBP_RSP: {
                    auto address = std::uint64_t(editable.get_memory().data() + relocation.address_offset);
                    futils::binary::writer mem{editable.get_memory()};
                    mem.reset(relocation.rewrite_offset);
                    write_save_instruction_pointer_rbp_rsp(mem, full_memory, address);
                    assert(mem.offset() == relocation.end_offset);
                    break;
                }
                default: {
                    break;
                }
            }
        }
        return editable.make_executable();
    }
}  // namespace brgen::vm2
