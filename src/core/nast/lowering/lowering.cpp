/*license*/
#include "lowering.hpp"

namespace brgen::nast::lowering {

    std::optional<std::uint64_t> byte_width(Context& c, Node<Type> t) {
        if (!t) {
            return std::nullopt;
        }
        // 整数は自分で幅を持っている。合成した型 (Builder::int_type が作る
        // もの) は TypeSize 表に載っていないので、表より先にこちらを見る。
        if (auto i = t.as_any<IntType>()) {
            auto bits = i.ref(c.a)->bit_size;
            if (bits == 0 || bits % 8 != 0) {
                return std::nullopt;
            }
            return bits / 8;
        }
        auto* s = c.tables.table<TypeSize>().get(t);
        if (!s || s->kind != SizeKind::fixed || s->bits % 8 != 0) {
            return std::nullopt;
        }
        return s->bits / 8;
    }

}  // namespace brgen::nast::lowering
