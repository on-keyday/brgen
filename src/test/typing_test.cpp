/*license*/
#include "ast_test_component.h"
#include <core/typing/typing.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, Input& input, std::string_view file) {
        ::testing::AssertionResult result = ::testing::AssertionFailure();
        try {
            typing::typing_object(a);
            result = ::testing::AssertionSuccess();
            // 例外がスローされる可能性のあるコード
        } catch (const typing::DefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string(file);
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::NotDefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` not defined", e.ref_at.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::AssignError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string(file);
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::NotBoolError& e) {
            auto err = input.error("expression is not a boolean", e.expr_loc.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::BinaryOpTypeError& e) {
            auto err = input.error("invalid operand types for binary operator", e.loc.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::NotEqualTypeError& e) {
            auto err = input.error("type not equal; here", e.a.pos).to_string(file);
            err += "\n" + input.error("and here", e.b.pos).to_string(file) + "\n";
            result << err;
        } catch (const typing::UnsupportedError& e) {
            auto err = input.error("unsupported operation", e.l.pos).to_string(file) + "\n";
            result << err;
        } catch (const std::exception& e) {
            // 他の標準の例外をキャッチする場合の処理
            // e.what() を使用して詳細情報を取得する
            result << e.what();
        } catch (...) {
            result << "unknown exception caught";
        }
        ASSERT_TRUE(result);
    });
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}