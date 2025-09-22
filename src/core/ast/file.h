/*license*/
#pragma once
#include "json.h"
#include <json/convert_json.h>
#include <memory>

namespace futils::comb2 {
    bool from_json(Pos& pos, const auto& j) {
        JSON_PARAM_BEGIN(pos, j)
        FROM_JSON_PARAM(begin, "begin")
        FROM_JSON_PARAM(end, "end")
        JSON_PARAM_END()
    }
}  // namespace futils::comb2

namespace brgen {
    bool from_json(SourceError& se, const auto& j) {
        JSON_PARAM_BEGIN(se, j)
        FROM_JSON_PARAM(errs, "errs")
        JSON_PARAM_END()
    }

    bool from_json(SourceEntry& se, const auto& j) {
        JSON_PARAM_BEGIN(se, j)
        FROM_JSON_PARAM(msg, "msg")
        FROM_JSON_PARAM(file, "file")
        FROM_JSON_PARAM(loc, "loc")
        FROM_JSON_PARAM(src, "src")
        FROM_JSON_PARAM(warn, "warn")
        JSON_PARAM_END()
    }

    namespace lexer {
        bool from_json(lexer::Token& t, const auto& j) {
            JSON_PARAM_BEGIN(t, j)
            FROM_JSON_PARAM(loc, "loc")
            FROM_JSON_PARAM(tag, "tag")
            FROM_JSON_PARAM(token, "token")
            JSON_PARAM_END()
        }

        bool from_json(lexer::Tag& tag, const auto& j) {
            std::string str;
            if (!futils::json::convert_from_json(str, j)) {
                return false;
            }
            auto opt = lexer::from_string<lexer::Tag>(str);
            if (!opt) {
                return false;
            }
            tag = *opt;
            return true;
        }

        bool from_json(lexer::Loc& loc, const auto& j) {
            JSON_PARAM_BEGIN(loc, j)
            FROM_JSON_PARAM(pos, "pos")
            FROM_JSON_PARAM(line, "line")
            FROM_JSON_PARAM(col, "col")
            FROM_JSON_PARAM(file, "file")
            JSON_PARAM_END()
        }

    }  // namespace lexer

    namespace ast {
        struct TokenFile {
            bool success = false;
            std::vector<std::string> files;
            std::optional<std::vector<lexer::Token>> tokens;
            std::optional<SourceError> error;

            bool from_json(const auto& j) {
                JSON_PARAM_BEGIN(*this, j)
                FROM_JSON_PARAM(success, "success")
                FROM_JSON_PARAM(files, "files")
                FROM_JSON_PARAM(tokens, "tokens")
                FROM_JSON_PARAM(error, "error")
                JSON_PARAM_END()
            }
        };

        struct AstFile {
            bool success = false;
            std::vector<std::string> files;
            std::optional<JSON> ast;
            std::optional<SourceError> error;

            bool from_json(const auto& j) {
                JSON_PARAM_BEGIN(*this, j)
                FROM_JSON_PARAM(success, "success")
                FROM_JSON_PARAM(files, "files")
                FROM_JSON_PARAM(ast, "ast")
                FROM_JSON_PARAM(error, "error")
                JSON_PARAM_END()
            }
        };

        // for C-API direct ast pass mode interface
        struct Program;
        struct DirectASTPassInterface {
            const std::vector<std::string>* const files = nullptr;
            const SourceError* const error = nullptr;
            const std::shared_ptr<Program>* const ast = nullptr;
        };
    }  // namespace ast
}  // namespace brgen
