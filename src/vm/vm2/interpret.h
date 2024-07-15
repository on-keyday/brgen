/*license*/
#pragma once
#include <binary/reader.h>
#include "vm_enum.h"
#include "vm2.h"
#include <unordered_map>
#include <binary/bit.h>
#include <list>

namespace brgen::vm2 {
    struct AllocationEntry {
        std::uint64_t address;
        std::uint64_t size;
        bool used = false;
    };

    struct VM2 {
       private:
        futils::view::wvec full_memory;
        size_t stack_size = 0;
        futils::binary::reader instructions{futils::view::rvec{}};
        std::uint64_t registers[size_t(Register::REGISTER_COUNT)] = {0};
        std::list<AllocationEntry> allocation_map;
        bool safe_call = false;

        std::uint64_t& get_register(Register reg) {
            return registers[size_t(reg)];
        }

        void set_trap(TrapNumber trap, std::uint64_t reason) {
            get_register(Register::TRAP) = std::uint64_t(trap);
            get_register(Register::TRAP_REASON) = reason;
        }

        friend struct VM2Helper;

       public:
        void reset(futils::view::rvec instructions, futils::view::wvec memory, size_t stack_size);

        void resume();

        std::optional<Inst> step();

        std::pair<std::optional<Inst>, size_t> decode_inst();

        void set_safe_call_mode(bool safe) {
            safe_call = safe;
        }

        void step_inst(const Inst& inst, size_t offset = 0);

        bool handle_syscall(futils::binary::bit_reader* input = nullptr, futils::binary::bit_writer* output = nullptr);

        TrapNumber get_trap() const {
            return TrapNumber(registers[size_t(Register::TRAP)]);
        }

        std::uint64_t get_trap_reason() const {
            return registers[size_t(Register::TRAP_REASON)];
        }

        void set_syscall_return(std::uint64_t value) {
            get_register(Register::R0) = value;
        }
    };
}  // namespace brgen::vm2
