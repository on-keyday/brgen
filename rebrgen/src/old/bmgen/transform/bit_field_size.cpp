/*license*/
#include <bmgen/internal.hpp>
#include <unordered_set>

namespace rebgn {
    struct InfiniteError {
        std::string message;

        void error(auto&& pb) {
            futils::strutil::append(pb, message);
        }
    };

    expected<size_t> decide_maximum_bit_field_size(Module& m, std::unordered_set<ObjectID>& searched, AbstractOp end_op, size_t index);

    expected<size_t> get_from_type(Module& m, const Storages& storage, std::unordered_set<ObjectID>& searched) {
        size_t factor = 1;
        for (size_t j = 0; j < storage.storages.size(); j++) {
            auto& s = storage.storages[j];
            if (s.type == StorageType::ARRAY) {
                factor *= s.size().value().value();
            }
            else if (s.type == StorageType::UINT || s.type == StorageType::INT) {
                factor *= s.size().value().value();
            }
            else if (s.type == StorageType::STRUCT_REF) {
                auto ref = s.ref().value().value();
                if (searched.find(ref) != searched.end()) {
                    return unexpect_error(InfiniteError{"Infinite: STRUCT_REF"});
                }
                searched.insert(ref);
                auto idx = m.ident_index_table[ref];
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_FORMAT, idx);
                searched.erase(ref);
                if (!size) {
                    return size.error();
                }
                factor *= size.value();
            }
            else if (s.type == StorageType::ENUM) {
                // skip
            }
            else if (s.type == StorageType::VARIANT) {
                size_t candidate = 0;
                for (j++; j < storage.storages.size(); j++) {
                    if (storage.storages[j].type != StorageType::STRUCT_REF) {
                        return unexpect_error("Invalid storage type: {}", to_string(storage.storages[j].type));
                    }
                    auto ref = storage.storages[j].ref().value().value();
                    if (searched.find(ref) != searched.end()) {
                        return unexpect_error(InfiniteError{"Infinite: VARIANT"});
                    }
                    searched.insert(ref);
                    auto idx = m.ident_index_table[ref];
                    auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_UNION_MEMBER, idx);
                    searched.erase(ref);
                    if (!size) {
                        return size.error();
                    }
                    candidate = std::max(candidate, size.value());
                }
                factor *= candidate;
            }
            else {
                return unexpect_error(InfiniteError{std::format("Invalid storage type: {}", to_string(s.type))});
            }
        }
        return factor;
    }

    expected<size_t> decide_maximum_bit_field_size(Module& m, std::unordered_set<ObjectID>& searched, AbstractOp end_op, size_t index) {
        size_t bit_size = 0;
        for (size_t i = index; m.code[i].op != end_op; i++) {
            if (m.code[i].op == AbstractOp::DEFINE_FIELD) {
                auto ref = *m.code[i].type();
                auto got = m.get_storage(ref);
                if (!got) {
                    return unexpect_error(std::move(got.error()));
                }
                auto size = get_from_type(m, got.value(), searched);
                if (!size) {
                    return size.error();
                }
                bit_size += size.value();
            }
            if (m.code[i].op == AbstractOp::DECLARE_BIT_FIELD) {
                auto index = m.ident_index_table[m.code[i].ref().value().value()];
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_BIT_FIELD, index);
                if (!size) {
                    return size.error();
                }
                bit_size += size.value();
            }
        }
        return bit_size;
    }

    Error decide_bit_field_size(Module& m) {
        std::unordered_set<ObjectID> searched;
        std::unordered_map<ObjectID, size_t> bit_fields;
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_BIT_FIELD) {
                auto size = decide_maximum_bit_field_size(m, searched, AbstractOp::END_BIT_FIELD, i);
                if (!size) {
                    if (!size.error().as<InfiniteError>()) {
                        return size.error();
                    }
                    continue;
                }
                bit_fields[m.code[i].ident().value().value()] = size.value();
            }
        }
        std::vector<Code> rebound;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_BIT_FIELD) {
                auto ident = c.ident().value().value();
                auto found = bit_fields.find(ident);
                if (found != bit_fields.end()) {
                    Storages storage;
                    Storage s;
                    s.type = StorageType::UINT;
                    s.size(*varint(found->second));
                    storage.storages.push_back(std::move(s));
                    storage.length = *varint(1);
                    auto ref = m.get_storage_ref(storage, nullptr);
                    if (!ref) {
                        return ref.error();
                    }
                    c.type(*ref);
                }
            }
            rebound.push_back(std::move(c));
        }
        m.code = std::move(rebound);
        return none;
    }

}