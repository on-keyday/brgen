/*license*/
#include "endian_variable.hpp"
#include "../node/build.h"
#include "../node/util.h"

#include <format>

namespace brgen::nast::lowering {

    LoweredEndianVariable* lower_endian_variable(Context& c, Node<SpecifyOrder> order) {
        if (!order) {
            return nullptr;
        }
        if (auto* got = c.tables.table<LoweredEndianVariable>().get(order)) {
            return got;  // 2 度目は同じ名前を返す
        }
        auto d = order.ref(c.a);
        if (d->name != "input.endian" || !d->order) {
            return nullptr;
        }
        Builder b{c.a, order.ref(c.a).loc()};
        auto name = c.a.make<Ident>(b.loc);
        name->identifier = derived_name("endian", order);

        LoweredEndianVariable lowered;
        lowered.name = name;
        lowered.value = d->order;  // 元の式をそのまま指す (複製しない)
        c.tables.table<LoweredEndianVariable>().set(order, std::move(lowered));
        return c.tables.table<LoweredEndianVariable>().get(order);
    }

}  // namespace brgen::nast::lowering
