/*license*/
#pragma once
#include "../lexer/token.h"
#include "../common/file.h"
#include "../ast/ast.h"

namespace brgen::typing {

    struct DefinedError {
        std::string ident;
        lexer::Loc duplicated_at;
        lexer::Loc defined_at;
    };

    struct NotDefinedError {
        std::string ident;
        lexer::Loc ref_at;
    };

    struct AssignError {
        std::string ident;
        lexer::Loc duplicated_at;
        lexer::Loc defined_at;
    };

    struct NotBoolError {
        lexer::Loc expr_loc;
    };

    struct BinaryOpTypeError {
        ast::BinaryOp op;
        lexer::Loc loc;
    };

    struct NotEqualTypeError {
        lexer::Loc a;
        lexer::Loc b;
    };

    struct UnsupportedError {
        lexer::Loc l;
    };

    struct TooLargeError {
        lexer::Loc l;
    };

    void typing_object(const std::shared_ptr<ast::Node>& ty);

    inline either::expected<void, std::string> typing_with_error(const std::shared_ptr<ast::Node>& ty, FileSet& input) {
        std::string err;
        try {
            typing_object(ty);
        } catch (const typing::DefinedError& e) {
            err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at).to_string();
            err += "\n" + input.error("defined at here", e.defined_at).to_string() + "\n";
        } catch (const typing::NotDefinedError& e) {
            err = input.error("ident `" + e.ident + "` not defined", e.ref_at).to_string() + "\n";
        } catch (const typing::AssignError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at).to_string();
            err += "\n" + input.error("defined at here", e.defined_at).to_string() + "\n";
        } catch (const typing::NotBoolError& e) {
            err = input.error("expression is not a boolean", e.expr_loc).to_string() + "\n";

        } catch (const typing::BinaryOpTypeError& e) {
            err = input.error("invalid operand types for binary operator", e.loc).to_string() + "\n";

        } catch (const typing::NotEqualTypeError& e) {
            err = input.error("type not equal; here", e.a).to_string();
            err += "\n" + input.error("and here", e.b).to_string() + "\n";

        } catch (const typing::UnsupportedError& e) {
            err = input.error("unsupported operation", e.l).to_string() + "\n";

        } catch (const std::exception& e) {
            // 他の標準の例外をキャッチする場合の処理
            // e.what() を使用して詳細情報を取得する
            err = e.what();
        } catch (...) {
            err = "unknown exception caught";
        }
        if (err.size()) {
            return either::unexpected{err};
        }
        return {};
    }
}  // namespace brgen::typing
