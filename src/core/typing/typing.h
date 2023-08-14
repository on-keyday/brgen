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

    inline std::optional<std::string> typing_with_error(const std::shared_ptr<ast::Node>& ty, Input& input) {
        try {
            typing_object(ty);
        } catch (const typing::DefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string();
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string() + "\n";
            return err;
        } catch (const typing::NotDefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` not defined", e.ref_at.pos).to_string() + "\n";
            return err;
        } catch (const typing::AssignError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string();
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string() + "\n";
            return err;
        } catch (const typing::NotBoolError& e) {
            auto err = input.error("expression is not a boolean", e.expr_loc.pos).to_string() + "\n";
            return err;
        } catch (const typing::BinaryOpTypeError& e) {
            auto err = input.error("invalid operand types for binary operator", e.loc.pos).to_string() + "\n";
            return err;
        } catch (const typing::NotEqualTypeError& e) {
            auto err = input.error("type not equal; here", e.a.pos).to_string();
            err += "\n" + input.error("and here", e.b.pos).to_string() + "\n";
            return err;
        } catch (const typing::UnsupportedError& e) {
            auto err = input.error("unsupported operation", e.l.pos).to_string() + "\n";
            return err;
        } catch (const std::exception& e) {
            // 他の標準の例外をキャッチする場合の処理
            // e.what() を使用して詳細情報を取得する
            return e.what();
        } catch (...) {
            return "unknown exception caught";
        }
        return std::nullopt;
    }
}  // namespace brgen::typing
