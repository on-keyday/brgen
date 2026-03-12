/*license*/
#pragma once

#include "ebmgen/mapping.hpp"
#include "json/stringer.h"
namespace ebmgen {
    struct JSONPrinter {
       public:
        JSONPrinter(const MappingTable& module);

        void print_module(futils::json::Stringer<>&);

        void print_object(futils::json::Stringer<>&, const ebm::Statement& obj);
        void print_object(futils::json::Stringer<>&, const ebm::Expression& obj);
        void print_object(futils::json::Stringer<>&, const ebm::Type& obj);
        void print_object(futils::json::Stringer<>&, const ebm::Identifier& obj);
        void print_object(futils::json::Stringer<>&, const ebm::StringLiteral& obj);

       private:
        futils::json::Stringer<>* os_;  // Reference to the output stream
        const MappingTable& module_;

        template <typename T>
        void print_value(const T& value) const;

        template <typename T>
        void print_named_value(auto&& object, const char* name, const T& value) const;

        template <typename T>
        void print_named_value(auto&& object, const char* name, const T* value) const;
    };
}  // namespace ebmgen