/*license*/
#pragma once
#include <jit/x64.h>
#include <jit/x64_coroutine.h>
#include <jit/jit_memory.h>
#include "interpret.h"
#include <string>
#include <platform/detect.h>
#include <bit>
#include <cstring>

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

    void VM2::jit_compile_inst(futils::binary::writer& w, std::vector<futils::jit::RelocationEntry>& relocations) {
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
                    break;
                }
                case Op2::SYSCALL_IMMEDIATE: {
                    futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, std::uint64_t(TrapNumber::SYSCALL));
                    auto rewrite_offset = w.offset();
                    futils::jit::x64::coro::write_save_instruction_pointer_rbp_rsp(w, full_memory, 0);
                    auto end_offset = w.offset();
                    futils::jit::x64::coro::write_coroutine_epilogue(w, full_memory);
                    auto address_offset = w.offset();
                    relocations.push_back({futils::jit::SAVE_RIP_RBP_RSP, rewrite_offset, end_offset, address_offset});
                    break;
                }
                case Op2::PUSH: {
                    auto push = *inst->push();
                    auto reg = map_register(push.operand);
                    futils::jit::x64::emit_push_reg(w, reg);
                    break;
                }
                case Op2::POP: {
                    auto pop = *inst->pop();
                    auto reg = map_register(pop.operand);
                    futils::jit::x64::emit_pop_reg(w, reg);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

    futils::jit::ExecutableMemory VM2::jit_compile() {
        std::string buffer;
        futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &buffer};
        futils::jit::x64::coro::write_coroutine_prologue(w, full_memory);
        std::vector<futils::jit::RelocationEntry> relocations;
        auto initial_rip_offset = w.offset();
        relocations.push_back({
            futils::jit::INITIAL_RIP,
            0,
            0,
            initial_rip_offset,
        });
        jit_compile_inst(w, relocations);
        futils::jit::x64::emit_mov_reg_imm(w, futils::jit::x64::Register::RAX, std::uint64_t(TrapNumber::END_OF_PROGRAM));
        auto rewrite_offset = w.offset();
        futils::jit::x64::coro::write_save_instruction_pointer(w, full_memory, 0);
        auto end_offset = w.offset();
        relocations.push_back({
            futils::jit::RESET_RIP,
            rewrite_offset,
            end_offset,
            initial_rip_offset,
        });
        futils::jit::x64::coro::write_reset_next_rbp_rsp(w, full_memory);
        futils::jit::x64::coro::write_coroutine_epilogue(w, full_memory);
        auto editable = futils::jit::EditableMemory::allocate(w.written().size());
        if (!editable.valid()) {
            return {};
        }
        memcpy(editable.get_memory().data(), w.written().data(), w.written().size());
        for (auto& relocation : relocations) {
            futils::jit::x64::coro::relocate(editable.get_memory(), relocation, full_memory);
        }
        return editable.make_executable();
    }
}  // namespace brgen::vm2
