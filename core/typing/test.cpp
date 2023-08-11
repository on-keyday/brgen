/*license*/
#include "../ast/test_component.h"
#include "typing.h"
using namespace brgen;

int main() {
    test_ast([](auto& a, Input& input) {
        try {
            typing::typing_object(a);
            // 例外がスローされる可能性のあるコード
        } catch (const typing::DefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string("file");
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string("file") + "\n";
            cerr << err;
        } catch (const typing::NotDefinedError& e) {
            auto err = input.error("ident `" + e.ident + "` not defined", e.ref_at.pos).to_string("file");
            cerr << err;
        } catch (const typing::AssignError& e) {
            auto err = input.error("ident `" + e.ident + "` already defined", e.duplicated_at.pos).to_string("file");
            err += "\n" + input.error("defined at here", e.defined_at.pos).to_string("file") + "\n";
            cerr << err;
        } catch (const typing::NotBoolError& e) {
            auto err = input.error("expression is not a boolean at", e.expr_loc.pos).to_string("file");
            cerr << err;
        } catch (const typing::BinaryOpTypeError& e) {
            auto err = input.error("invalid operand types for binary operator", e.loc.pos).to_string("file");
            cerr << err;
        } catch (const typing::NotEqualTypeError& e) {
            auto err = input.error("type not equal", e.a.pos).to_string("file");
            err += "\n" + input.error("type not equal", e.a.pos).to_string("file");
            cerr << err;
        } catch (const std::exception& e) {
            // 他の標準の例外をキャッチする場合の処理
            // e.what() を使用して詳細情報を取得する
        } catch (...) {
            // 上記の例外以外のすべての例外をキャッチする場合の処理
        }
    });
}