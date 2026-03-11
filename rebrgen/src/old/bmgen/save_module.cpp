/*license*/
#include "convert.hpp"

namespace rebgn {
    Error save(Module& m, futils::binary::writer& w) {
        BinaryModule bm;
        auto id = varint(m.object_id - 1);
        if (!id) {
            return id.error();
        }
        bm.max_id = id.value();

        auto add_refs = [&](StringRefs& refs, std::uint64_t id, std::string_view str) {
            StringRef sr;
            auto idv = varint(id);
            if (!idv) {
                return idv.error();
            }
            sr.code = idv.value();
            auto length = varint(str.size());
            if (!length) {
                return length.error();
            }
            sr.string.length = length.value();
            sr.string.data = str;
            refs.refs.push_back(std::move(sr));
            return none;
        };

        auto process_table = [&](auto& table, StringRefs& refs) -> Error {
            for (auto& [key, id] : table) {
                auto err = add_refs(refs, id, key);
                if (err) {
                    return err;
                }
            }
            auto length = varint(refs.refs.size());
            if (!length) {
                return length.error();
            }
            refs.refs_length = length.value();
            return none;
        };

        if (auto err = process_table(m.metadata_table, bm.metadata); err) {
            return err;
        }

        if (auto err = process_table(m.string_table, bm.strings); err) {
            return err;
        }

        for (auto& [ident, id] : m.ident_table) {
            auto err = add_refs(bm.identifiers, id, ident->ident);
            if (err) {
                return err;
            }
        }
        auto length = varint(bm.identifiers.refs.size());
        if (!length) {
            return length.error();
        }
        bm.identifiers.refs_length = length.value();

        for (auto& [ident, index] : m.ident_index_table) {
            auto id = varint(ident);
            if (!id) {
                return id.error();
            }
            auto idx = varint(index);
            if (!idx) {
                return idx.error();
            }
            IdentIndex ii;
            ii.ident = id.value();
            ii.index = idx.value();
            bm.ident_indexes.refs.push_back(std::move(ii));
        }
        length = varint(bm.ident_indexes.refs.size());
        if (!length) {
            return length.error();
        }
        bm.ident_indexes.refs_length = length.value();

        for (auto& s : m.storage_key_table_rev) {
            auto found = m.storage_table.find(s.second);
            if (found == m.storage_table.end()) {
                return error("Storage key not found: {}", s.second);
            }
            auto key = varint(s.first);
            if (!key) {
                return key.error();
            }
            bm.types.maps.push_back({
                .code = key.value(),
                .storage = std::move(found->second),
            });
        }
        length = varint(bm.types.maps.size());
        if (!length) {
            return length.error();
        }
        bm.types.length = length.value();

        auto process_ranges = [&](auto& ranges, auto& bm_ranges) -> Error {
            auto length = varint(ranges.size());
            if (!length) {
                return length.error();
            }
            bm_ranges.length = length.value();
            bm_ranges.ranges = std::move(ranges);
            return none;
        };

        if (auto err = process_ranges(m.programs, bm.programs); err) {
            return err;
        }

        if (auto err = process_ranges(m.ident_to_ranges, bm.ident_ranges); err) {
            return err;
        }

        auto code_length = varint(m.code.size());
        if (!code_length) {
            return code_length.error();
        }
        bm.code_length = code_length.value();
        bm.code = m.code;

        return bm.encode(w);
    }
}  // namespace rebgn
