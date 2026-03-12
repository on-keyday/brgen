#pragma once

#include <iostream>
#include <ostream>

#include "ebm/extended_binary_module.hpp"
#include "mapping.hpp"

namespace ebmgen {

    class DebugPrinter {
       public:
        DebugPrinter(const MappingTable& module, std::ostream& os);

        void print_module() const;

        void print_object(const ebm::Identifier& obj) const;
        void print_object(const ebm::StringLiteral& obj) const;
        void print_object(const ebm::Type& obj) const;
        void print_object(const ebm::Statement& obj) const;
        void print_object(const ebm::Expression& obj) const;

       private:
        std::ostream& os_;  // Reference to the output stream
        const MappingTable& module_;

        void print_resolved_reference(const ebm::IdentifierRef& ref) const;
        void print_resolved_reference(const ebm::StringRef& ref) const;
        void print_resolved_reference(const ebm::TypeRef& ref) const;
        void print_resolved_reference(const ebm::StatementRef& ref) const;
        void print_resolved_reference(const ebm::ExpressionRef& ref) const;
        void print_resolved_reference(const ebm::AnyRef& ref) const;

        void print_any_ref(auto ref) const;

        // Indentation helper
        mutable int indent_level_ = 0;
        void indent() const {
            for (int i = 0; i < indent_level_; ++i) {
                os_ << "  ";  // Write to the provided output stream
            }
        }

        template <typename T>
        void print_value(const T& value) const;

        template <typename T>
        void print_named_value(const char* name, const T& value) const;

        template <typename T>
        void print_named_value(const char* name, const T* value) const;
    };

}  // namespace ebmgen