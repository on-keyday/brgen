
#include "define_visitor.hpp"
DEFINE_VISITOR(Self) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        // 符号化中の値そのもの。綴りは言語ごとなので knob から取る。
        auto& spelling = ctx.config().Self.spelling;
        if (spelling.empty()) {
            HANDLE_UNHANDLED();
        }
        return CODE_AT(node, spelling);
    }
    ON_UNHANDLED_DEFAULT()
}
