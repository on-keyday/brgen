/*license*/
#include <bmgen/internal.hpp>
#include <core/ast/tool/sort.h>

namespace rebgn {
    Error sort_formats(Module& m, const std::shared_ptr<ast::Node>& node) {
        ast::tool::FormatSorter sorter;
        auto sorted_formats = sorter.topological_sort(node);
        std::unordered_map<ObjectID, std::shared_ptr<ast::Format>> sorted;
        for (auto& f : sorted_formats) {
            auto ident = m.lookup_ident(f->ident);
            if (!ident) {
                return ident.error();
            }
            sorted[ident->value()] = f;
        }
        std::vector<Range> programs;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROGRAM) {
                auto start = i;
                for (; i < m.code.size(); i++) {
                    if (m.code[i].op == AbstractOp::END_PROGRAM) {
                        break;
                    }
                }
                programs.push_back({start, i + 1});
            }
        }
        struct RangeOrder {
            Range range;
            size_t order;
        };
        std::vector<RangeOrder> programs_order;
        for (auto p : programs) {
            std::vector<Code> reordered;
            std::vector<Code> formats;
            for (size_t i = p.start; i < p.end; i++) {
                if (m.code[i].op == AbstractOp::DECLARE_FORMAT) {
                    formats.push_back(std::move(m.code[i]));
                }
                else {
                    reordered.push_back(std::move(m.code[i]));
                }
            }
            size_t earliest = -1;
            std::ranges::sort(formats, [&](auto& a, auto& b) {
                auto ident_a = a.ref().value().value();
                auto ident_b = b.ref().value().value();
                auto found_a = sorted.find(ident_a);
                auto found_b = sorted.find(ident_b);
                if (found_a == sorted.end() || found_b == sorted.end()) {
                    return false;
                }
                auto index_a = std::find(sorted_formats.begin(), sorted_formats.end(), found_a->second);
                auto index_b = std::find(sorted_formats.begin(), sorted_formats.end(), found_b->second);
                if (earliest > index_a - sorted_formats.begin()) {
                    earliest = index_a - sorted_formats.begin();
                }
                if (earliest > index_b - sorted_formats.begin()) {
                    earliest = index_b - sorted_formats.begin();
                }
                return index_a < index_b;
            });
            auto end_program = std::move(reordered.back());
            reordered.pop_back();
            reordered.insert(reordered.end(), formats.begin(), formats.end());
            reordered.push_back(std::move(end_program));
            for (size_t i = p.start; i < p.end; i++) {
                m.code[i] = std::move(reordered[i - p.start]);
            }
            programs_order.push_back({
                .range = p,
                .order = earliest,
            });
        }
        std::ranges::sort(programs_order, [](auto& a, auto& b) {
            return a.order < b.order;
        });
        std::vector<Code> reordered;
        // first, append programs
        for (auto& p : programs_order) {
            auto start = reordered.size();
            for (size_t i = p.range.start; i < p.range.end; i++) {
                reordered.push_back(std::move(m.code[i]));
            }
            auto end = reordered.size();
            auto startV = varint(start);
            auto endV = varint(end);
            if (!startV || !endV) {
                return error("Failed to convert varint");
            }
            m.programs.push_back({*startV, *endV});
        }
        // then, append the rest
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_PROGRAM) {
                for (; i < m.code.size(); i++) {
                    if (m.code[i].op == AbstractOp::END_PROGRAM) {
                        break;
                    }
                }
                continue;
            }
            if (m.code[i].op == AbstractOp::DECLARE_PROGRAM) {  // this is needless so remove
                continue;
            }
            reordered.push_back(std::move(m.code[i]));
        }
        m.code = std::move(reordered);
        return none;
    }

}