/*license*/
#pragma once
#include <variant>
#include "../ast.h"

namespace brgen::ast::tool {

    enum class EResultType {
        boolean,
        integer,
        string,
        ident,
    };

    struct EResult {
       private:
        std::variant<bool, std::int64_t, std::string, std::string> value;

       public:
    };

    struct Evaluator {
       private:
       public:
    };

}  // namespace brgen::ast::tool