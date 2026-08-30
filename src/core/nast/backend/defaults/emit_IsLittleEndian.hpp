
#include "define_visitor.hpp"
DEFINE_VISITOR(IsLittleEndian) {
    DEFAULT_HANDLER()
    ON_CODEGEN() {
        auto& knobs = ctx.config().IsLittleEndian;
        if (auto order = node.ref(ctx.a)->order) {
            // 実行時に決まる順。代入の位置で作った変数を読んで比べる。名前は
            // その代入から決まる (lowering/endian_variable が表に置く)。表に
            // 無ければ、まだ誰も代入を降ろしていないということなので断る。
            auto* var = ctx.t.template table<LoweredEndianVariable>().get(order);
            if (!var || knobs.little_endian_value.empty()) {
                HANDLE_UNHANDLED();
            }
            return CODE_AT(node, ident_text(ctx.a, var->name), " == ", knobs.little_endian_value);
        }
        // ターゲット上の静的な値。綴りは言語ごとに違うので knob から取る。
        // 空なら黙って big と決めつけず、未対応として音を鳴らす。
        if (knobs.native_endian_check.empty()) {
            HANDLE_UNHANDLED();
        }
        return CODE_AT(node, knobs.native_endian_check);
    }
    ON_UNHANDLED_DEFAULT()
}
