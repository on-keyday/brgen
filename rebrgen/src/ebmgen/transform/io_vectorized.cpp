/*license*/
#include "ebmgen/converter.hpp"
#include "transform.hpp"
#include "../convert/helper.hpp"
namespace ebmgen {
    ebm::Block* get_block(ebm::StatementBody& body) {
        ebm::Block* block = nullptr;
        body.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
            if constexpr (std::is_same_v<std::decay_t<decltype(value)>, ebm::Block*>) {
                block = value;
            }
            else
                VISITOR_RECURSE(visitor, name, value)
        });
        return block;
    }

    expected<void> vectorized_io(TransformContext& tctx, bool write) {
        // Implementation of the grouping I/O transformation
        auto& all_statements = tctx.statement_repository().get_all();
        const auto current_added = all_statements.size();
        const auto current_alias = tctx.alias_vector().size();
        std::map<size_t, std::vector<std::pair<std::pair<size_t, size_t>, std::function<expected<ebm::StatementRef>()>>>> update;
        for (size_t i = 0; i < current_added; ++i) {
            auto block = get_block(all_statements[i].body);
            if (!block) {
                continue;
            }
            std::vector<std::tuple<size_t /*index in block*/, ebm::StatementRef, ebm::IOData*>> io;
            std::vector<std::vector<std::tuple<size_t, ebm::StatementRef, ebm::IOData*>>> ios;
            for (size_t j = 0; j < block->container.size(); j++) {
                auto id = block->container[j];
                MAYBE(stmt, tctx.statement_repository().get(id));
                if (auto n = (write ? stmt.body.write_data() : stmt.body.read_data()); n && (n->size.unit == ebm::SizeUnit::BIT_FIXED || n->size.unit == ebm::SizeUnit::BYTE_FIXED)) {
                    io.push_back({j, id, n});
                }
                else {
                    if (io.size() > 1) {
                        ios.push_back(std::move(io));
                    }
                    io.clear();
                }
            }
            if (io.size() > 1) {
                ios.push_back(std::move(io));
            }
            if (ios.size()) {
                print_if_verbose(write ? "Write" : "Read", " I/O groups:", ios.size(), "\n");
                for (auto& g : ios) {
                    print_if_verbose("  Group size: ", g.size(), "\n");
                    std::optional<std::uint64_t> all_in_byte = 0;
                    std::uint64_t all_in_bits = 0;
                    for (auto& ref : g) {
                        print_if_verbose("    - Statement ID: ", get_id(std::get<1>(ref)), "\n");
                        print_if_verbose("    - Size: ", std::get<2>(ref)->size.size()->value(), " ", to_string(std::get<2>(ref)->size.unit), "\n");
                        if (all_in_byte) {
                            if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                                all_in_byte = all_in_byte.value() + std::get<2>(ref)->size.size()->value();
                            }
                            else {
                                all_in_byte = std::nullopt;  // Mixed units, cannot use byte
                            }
                        }
                        if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BIT_FIXED) {
                            all_in_bits += std::get<2>(ref)->size.size()->value();
                        }
                        else if (std::get<2>(ref)->size.unit == ebm::SizeUnit::BYTE_FIXED) {
                            all_in_bits += std::get<2>(ref)->size.size()->value() * 8;
                        }
                        else {
                            return unexpect_error("Unsupported size unit in read I/O: {}", to_string(std::get<2>(ref)->size.unit));
                        }
                    }
                    ebm::Size total_size;
                    ebm::TypeRef data_typ;
                    if (all_in_byte) {
                        MAYBE(fixed_bytes, make_fixed_size(*all_in_byte, ebm::SizeUnit::BYTE_FIXED));
                        total_size = fixed_bytes;
                        auto& ctx = tctx.context();
                        EBMU_U8_N_ARRAY(typ, *all_in_byte, write ? ebm::ArrayAnnotation::write_temporary : ebm::ArrayAnnotation::read_temporary);
                        data_typ = typ;
                    }
                    else {
                        if (all_in_bits % 8 == 0) {
                            MAYBE(fixed_bytes, make_fixed_size(all_in_bits / 8, ebm::SizeUnit::BYTE_FIXED));
                            total_size = fixed_bytes;
                            auto& ctx = tctx.context();
                            EBMU_U8_N_ARRAY(typ, all_in_bits / 8, write ? ebm::ArrayAnnotation::write_temporary : ebm::ArrayAnnotation::read_temporary);
                            data_typ = typ;
                        }
                        else {
                            MAYBE(fixed_bits, make_fixed_size(all_in_bits, ebm::SizeUnit::BIT_FIXED));
                            total_size = fixed_bits;
                            auto& ctx = tctx.context();
                            EBMU_UINT_TYPE(typ, all_in_bits);
                            data_typ = typ;
                        }
                    }
                    print_if_verbose("Total size: ", total_size.size()->value(), " ", to_string(total_size.unit), "\n");
                    auto original_field = std::get<2>(g.front())->field;
                    auto& ctx = tctx.context();
                    MAYBE(t, ctx.repository().get_statement(from_weak(original_field)));
                    if (auto field_decl = t.body.field_decl()) {
                        if (auto comp = field_decl->composite_field()) {
                            original_field = *comp;
                        }
                    }
                    auto io_data = make_io_data(std::get<2>(g[0])->io_ref, from_weak(original_field), {}, data_typ, {}, total_size);
                    ebm::Block original_io;
                    for (auto& ref : g) {
                        append(original_io, std::get<1>(ref));
                    }
                    std::pair<size_t, size_t> group_range = {std::get<0>(g.front()), std::get<0>(g.back())};
                    update[i].emplace_back(group_range, [=, &tctx, original_io = std::move(original_io), io_data = std::move(io_data)]() mutable -> expected<ebm::StatementRef> {
                        auto& ctx = tctx.context();

                        EBM_BLOCK(original_, std::move(original_io));
                        io_data.attribute.has_lowered_statement(true);
                        io_data.lowered_statement(make_lowered_statement(ebm::LoweringIOType::VECTORIZED_IO, original_));
                        if (write) {
                            EBM_WRITE_DATA(vectorized_write, std::move(io_data));
                            return vectorized_write;
                        }
                        else {
                            EBM_READ_DATA(vectorized_read, std::move(io_data));
                            return vectorized_read;
                        }
                    });
                }
            }
        }
        std::map<std::uint64_t, ebm::StatementRef> map_statements;
        for (auto& [block_id, updates] : update) {
            auto stmt_id = tctx.statement_repository().get_all()[block_id].id;
            std::vector<std::pair<std::pair<size_t, size_t>, ebm::StatementRef>> new_statements;
            for (auto& [group_range, updater] : updates) {
                MAYBE(new_stmt, updater());
                print_if_verbose("Adding vectorized I/O statement for block ", get_id(stmt_id), ": ", get_id(new_stmt), "\n");
                new_statements.emplace_back(group_range, std::move(new_stmt));
            }
            auto block = get_block(tctx.statement_repository().get_all()[block_id].body);  // refetch because memory may be relocated
            assert(block);
            ebm::Block updated_block;
            size_t g = 0;
            for (size_t b = 0; b < block->container.size(); ++b) {
                if (g < new_statements.size() && new_statements[g].first.first == b) {
                    append(updated_block, new_statements[g].second);
                    b = new_statements[g].first.second;
                    g++;
                    continue;
                }
                append(updated_block, block->container[b]);
            }
            auto& ctx = tctx.context();
            EBM_BLOCK(new_block, std::move(updated_block));
            map_statements.emplace(get_id(stmt_id), new_block);
        }
        for (size_t i = 0; i < current_added; i++) {
            auto& stmt = tctx.statement_repository().get_all()[i];
            stmt.body.visit([&](auto&& visitor, const char* name, auto&& value) {
                if constexpr (std::is_same_v<ebm::StatementRef&, decltype(value)>) {
                    auto found = map_statements.find(get_id(value));
                    if (found != map_statements.end()) {
                        print_if_verbose("map old to new: ", get_id(value), " -> ", get_id(found->second), "\n");
                        value = found->second;
                    }
                }
                else
                    VISITOR_RECURSE_ARRAY(visitor, name, value)
                else VISITOR_RECURSE(visitor, name, value)
            });
        }
        for (size_t i = 0; i < current_alias; i++) {
            auto& alias = tctx.alias_vector()[i];
            if (alias.hint == ebm::AliasHint::STATEMENT) {
                auto found = map_statements.find(get_id(alias.to));
                if (found != map_statements.end()) {
                    print_if_verbose("map old to new: ", get_id(alias.to), " -> ", get_id(found->second), "\n");
                    alias.to.id = found->second.id;
                }
            }
        }
        for (auto& dbg : tctx.debug_locations()) {
            auto found = map_statements.find(get_id(dbg.ident));
            if (found != map_statements.end()) {
                dbg.ident.id = found->second.id;
            }
        }
        return {};
    }
}  // namespace ebmgen