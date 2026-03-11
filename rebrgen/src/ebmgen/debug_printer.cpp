#include "debug_printer.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include "ebm/extended_binary_module.hpp"
#include "escape/escape.h"
#include "helper/template_instance.h"
#include "common.hpp"
#include "number/to_string.h"
#include "unicode/utf/utf8.h"

namespace ebmgen {

    // Constructor: Initializes maps for quick lookups
    DebugPrinter::DebugPrinter(const MappingTable& module, std::ostream& os)
        : module_(module), os_(os) {
    }

    void DebugPrinter::print_resolved_reference(const ebm::IdentifierRef& ref) const {
        const auto* ident = module_.get_identifier(ref);
        os_ << ebm::Identifier::visitor_name << " ";
        if (ident) {
            os_ << ident->body.data;
        }
        else {
            os_ << "(unknown identifier)";
        }
    }
    void print_escaped_string(std::ostream& os_, std::string_view str) {
        os_ << "\"" << futils::escape::escape_str<std::string>(str, futils::escape::EscapeFlag::all) << "\"";
    }

    void DebugPrinter::print_resolved_reference(const ebm::StringRef& ref) const {
        os_ << ebm::StringLiteral::visitor_name << " ";
        const auto* str_lit = module_.get_string_literal(ref);
        if (str_lit) {
            print_escaped_string(os_, str_lit->body.data);
        }
        else {
            os_ << "(unknown string literal)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::TypeRef& ref) const {
        os_ << ebm::Type::visitor_name << " ";
        const auto* type = module_.get_type(ref);
        if (type) {
            os_ << to_string(type->body.kind);
            if (auto size = type->body.size()) {
                os_ << " size:" << size->value();
            }
            if (auto length = type->body.length()) {
                os_ << " length:" << length->value();
            }
            if (auto id = type->body.id()) {
                os_ << " ";
                print_resolved_reference(from_weak(*id));
            }
        }
        else {
            os_ << "(unknown type)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::StatementRef& ref) const {
        os_ << ebm::Statement::visitor_name << " ";
        const auto* stmt = module_.get_statement(ref);
        if (stmt) {
            os_ << to_string(stmt->body.kind);
            auto ident = module_.get_identifier(stmt->id);
            if (ident) {
                os_ << " name:";
                os_ << ident->body.data;
            }
            if (auto expr = stmt->body.expression()) {
                os_ << " ";
                print_resolved_reference(*expr);
            }
        }
        else {
            os_ << "(unknown statement)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::ExpressionRef& ref) const {
        os_ << ebm::Expression::visitor_name << " ";
        const auto* expr = module_.get_expression(ref);
        if (expr) {
            os_ << to_string(expr->body.kind);
            if (auto id = expr->body.id()) {
                os_ << " ";
                print_resolved_reference(from_weak(*id));
            }
            else if (auto member = expr->body.member()) {
                if (auto base = expr->body.base()) {
                    os_ << " base:";
                    print_resolved_reference(*base);
                }
                else if (auto enum_decl = expr->body.enum_decl()) {
                    os_ << " enum_decl:";
                    print_resolved_reference(*enum_decl);
                }
                os_ << " member:";
                print_resolved_reference(*member);
            }
            else if (auto lit = expr->body.string_value()) {
                os_ << " ";
                print_resolved_reference(*lit);
            }
            else if (auto lit = expr->body.int64_value()) {
                os_ << " value:" << *lit;
            }
            else if (auto lit = expr->body.int_value()) {
                os_ << " value:" << lit->value();
            }
            else if (auto lit = expr->body.type_ref()) {
                os_ << " ";
                print_resolved_reference(*lit);
            }
            else if (auto lit = expr->body.char_value()) {
                std::string v;
                futils::unicode::utf8::from_utf32(lit->value(), v, true);
                os_ << " " << v << "(code: U+" << futils::number::to_string<std::string>(lit->value(), 16, futils::number::ToStrFlag::upper) << ")";
            }
        }
        else {
            os_ << "(unknown expression)";
        }
    }
    void DebugPrinter::print_resolved_reference(const ebm::AnyRef& ref) const {
        if (const auto* ident = module_.get_identifier(ebm::IdentifierRef{ref.id})) {
            print_resolved_reference(ebm::IdentifierRef{ref.id});
        }
        else if (const auto* str_lit = module_.get_string_literal(ebm::StringRef{ref.id})) {
            print_resolved_reference(ebm::StringRef{ref.id});
        }
        else if (const auto* type = module_.get_type(ebm::TypeRef{ref.id})) {
            print_resolved_reference(ebm::TypeRef{ref.id});
        }
        else if (const auto* stmt = module_.get_statement(ebm::StatementRef{ref.id})) {
            print_resolved_reference(ebm::StatementRef{ref.id});
        }
        else if (const auto* expr = module_.get_expression(ebm::ExpressionRef{ref.id})) {
            print_resolved_reference(ebm::ExpressionRef{ref.id});
        }
        else {
            os_ << "(unknown reference)";
        }
    }

    void DebugPrinter::print_any_ref(auto value) const {
        os_ << " (ID: " << get_id(value) << " ";
        if (is_nil(value)) {
            os_ << "(null)";
        }
        else {
            print_resolved_reference(value);
        }
        os_ << ")";
    }

    // --- Generic print helpers ---
    template <typename T>
    void DebugPrinter::print_value(const T& value) const {
        if constexpr (std::is_enum_v<T>) {
            os_ << ebm::visit_enum(value) << " " << ebm::to_string(value) << "(" << std::uint64_t(value) << ")" << "\n";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            os_ << (value ? "true" : "false") << "\n";
        }
        else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
            if (value.empty()) {
                os_ << "(empty vector)\n";
                return;
            }
            os_ << "\n";
            indent_level_++;
            for (const auto& elem : value) {
                indent();
                print_value(elem);
            }
            indent_level_--;
        }
        else if constexpr (std::is_same_v<T, ebm::Varint>) {
            os_ << value.value() << "\n";
        }
        else if constexpr (has_visit<T, DummyFn>) {
            os_ << value.visitor_name;
            if constexpr (AnyRef<T>) {
                print_any_ref(value);
                os_ << "\n";
            }
            else {
                os_ << "\n";
                indent_level_++;
                value.visit([&](auto&& visitor, const char* name, auto&& val) {
                    print_named_value(name, val);
                });
                indent_level_--;
            }
        }
        else if constexpr (std::is_same_v<T, std::uint8_t>) {
            os_ << static_cast<uint32_t>(value) << "\n";
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            print_escaped_string(os_, value);
            os_ << "\n";
        }
        else {
            os_ << value << "\n";
        }
    }

    template <typename T>
    void DebugPrinter::print_named_value(const char* name, const T& value) const {
        indent();
        os_ << name << ": ";
        print_value(value);
    }

    template <typename T>
    void DebugPrinter::print_named_value(const char* name, const T* value) const {
        if (value) {
            if constexpr (std::is_same_v<T, char>) {
                print_named_value(name, std::string(value));
            }
            else {
                print_named_value(name, *value);
            }
        }
    }

    // --- Main print methods ---
    void DebugPrinter::print_module() const {
        if (module_.module().origin) {
            print_value(*module_.module().origin);
        }
        os_ << "Inverse references:\n";
        indent_level_++;
        for (std::uint64_t i = 1; i <= get_id(module_.module().max_id); ++i) {
            print_any_ref(ebm::AnyRef{i});
            auto found = module_.get_inverse_ref(ebm::AnyRef{i});
            if (!found) {
                os_ << ": (no references)\n";
                continue;
            }
            os_ << ":\n";
            indent_level_++;
            for (const auto& ref : *found) {
                indent();
                print_any_ref(ref.ref);
                os_ << " (from: " << ref.name;
                if (ref.index) {
                    os_ << "[" << *ref.index << "]";
                }
                os_ << ", hint: " << to_string(ref.hint) << ")\n";
            }
            indent_level_--;
        }
        indent_level_--;
    }

    void DebugPrinter::print_object(const ebm::Identifier& ref) const {
        print_value(ref);
    }
    void DebugPrinter::print_object(const ebm::StringLiteral& ref) const {
        print_value(ref);
    }
    void DebugPrinter::print_object(const ebm::Type& ref) const {
        print_value(ref);
    }
    void DebugPrinter::print_object(const ebm::Statement& ref) const {
        print_value(ref);
    }
    void DebugPrinter::print_object(const ebm::Expression& ref) const {
        print_value(ref);
    }

}  // namespace ebmgen