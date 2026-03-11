/*license*/
#pragma once

#include "ebmgen/mapping.hpp"
#include <set>

namespace ebmgen {
    void interactive_debugger(MappingTable& table);
    using ObjectResult = std::vector<ebm::AnyRef>;
    enum class ExecutionResult {
        Success,
        Exit,
        StackEmpty,
        TypeMismatch,
        InvalidOperator,
        IdentifierNotFound,
        InconsistentStack,
    };

    constexpr const char* to_string(ExecutionResult r) {
        switch (r) {
            case ExecutionResult::Success:
                return "Success";
            case ExecutionResult::Exit:
                return "Exit";
            case ExecutionResult::StackEmpty:
                return "StackEmpty";
            case ExecutionResult::TypeMismatch:
                return "TypeMismatch";
            case ExecutionResult::InvalidOperator:
                return "InvalidOperator";
            case ExecutionResult::IdentifierNotFound:
                return "IdentifierNotFound";
            case ExecutionResult::InconsistentStack:
                return "InconsistentStack";
            default:
                return "Unknown";
        }
    }
    using Failures = std::set<ExecutionResult>;
    expected<std::pair<ObjectResult, Failures>> run_query(MappingTable& table, std::string_view input);
}  // namespace ebmgen
