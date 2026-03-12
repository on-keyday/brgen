/*license*/
#pragma once
#include "internal.hpp"

namespace rebgn {
    Error add_endian_specific(Module& m, EndianExpr endian, auto&& op, auto&& on_little_endian, auto&& on_big_endian) {
        auto is_native_or_dynamic = endian.endian() == Endian::native || endian.endian() == Endian::dynamic;
        if (is_native_or_dynamic) {
            auto is_little = m.new_id(nullptr);
            if (!is_little) {
                return is_little.error();
            }
            op(AbstractOp::IS_LITTLE_ENDIAN, [&](Code& m) {
                m.ident(*is_little);
                m.ref(endian.dynamic_ref);  // dynamic endian or null for native
            });
            BM_REF(op, AbstractOp::IF, *is_little);
        }
        if (endian.endian() == Endian::little || is_native_or_dynamic) {
            auto err = on_little_endian();
            if (err) {
                return err;
            }
        }
        if (is_native_or_dynamic) {
            op(AbstractOp::ELSE, [&](Code& m) {});
        }
        if (endian.endian() == Endian::big || is_native_or_dynamic) {
            auto err = on_big_endian();
            if (err) {
                return err;
            }
        }
        if (is_native_or_dynamic) {
            op(AbstractOp::END_IF, [&](Code& m) {});
        }
        return none;
    }
#define NEW_CODE_OP(new_code)                                                                                                           \
    auto op = [&](AbstractOp o, auto&& set) {                                                                                           \
        new_code.push_back(make_code(o, set));                                                                                          \
    };                                                                                                                                  \
    auto new_var = [&](StorageRef ref, Varint new_id) {                                                                                 \
        auto new_var = m.new_id(nullptr);                                                                                               \
        if (!new_var) {                                                                                                                 \
            return new_var;                                                                                                             \
        }                                                                                                                               \
        op(AbstractOp::DEFINE_VARIABLE, [&](Code& m) {                                                                                  \
            m.ident(*new_var);                                                                                                          \
            m.ref(new_id);                                                                                                              \
            m.type(ref);                                                                                                                \
        });                                                                                                                             \
        return new_var;                                                                                                                 \
    };                                                                                                                                  \
    auto new_default_var = [&](StorageRef ref) {                                                                                        \
        auto new_id = m.new_id(nullptr);                                                                                                \
        if (!new_id) {                                                                                                                  \
            return new_id;                                                                                                              \
        }                                                                                                                               \
        op(AbstractOp::NEW_OBJECT, [&](Code& m) {                                                                                       \
            m.ident(*new_id);                                                                                                           \
            m.type(ref);                                                                                                                \
        });                                                                                                                             \
        return new_var(ref, *new_id);                                                                                                   \
    };                                                                                                                                  \
    auto new_array = [&](std::uint64_t len) {                                                                                           \
        Storages storage{.length = *varint(2), .storages = {Storage{.type = StorageType::ARRAY}, Storage{.type = StorageType::UINT}}};  \
        storage.storages[0].size(*varint(len));                                                                                         \
        storage.storages[1].size(*varint(8));                                                                                           \
        auto ref = m.get_storage_ref(storage, nullptr);                                                                                 \
        if (!ref) {                                                                                                                     \
            return ref.transform([](auto&&) { return Varint(); });                                                                      \
        }                                                                                                                               \
        return new_default_var(*ref);                                                                                                   \
    };                                                                                                                                  \
    auto new_vector = [&](Varint len_ref) {                                                                                             \
        Storages storage{.length = *varint(2), .storages = {Storage{.type = StorageType::VECTOR}, Storage{.type = StorageType::UINT}}}; \
        storage.storages[1].size(*varint(8));                                                                                           \
        auto ref = m.get_storage_ref(storage, nullptr);                                                                                 \
        if (!ref) {                                                                                                                     \
            return ref.transform([](auto&&) { return Varint(); });                                                                      \
        }                                                                                                                               \
        auto v = new_default_var(*ref);                                                                                                 \
        if (!v) {                                                                                                                       \
            return v;                                                                                                                   \
        }                                                                                                                               \
        if (len_ref.value() != null_id) {                                                                                               \
            op(AbstractOp::RESERVE_SIZE, [&](auto& c__) {                                                                               \
                c__.left_ref(*v);                                                                                                       \
                c__.right_ref(len_ref);                                                                                                 \
                c__.reserve_type(ReserveType::DYNAMIC);                                                                                 \
            });                                                                                                                         \
        }                                                                                                                               \
        return v;                                                                                                                       \
    };

    inline auto nbit_type_getter(Module& m, std::optional<StorageRef>& u8_typ) {
        return [&](size_t nbit_typ, bool sign) -> expected<StorageRef> {
            if (nbit_typ == 8 && !sign && u8_typ) {
                return *u8_typ;
            }
            Storage storage{.type = sign ? StorageType::INT : StorageType::UINT};
            storage.size(*varint(nbit_typ));
            auto res = m.get_storage_ref(Storages{.length = *varint(1), .storages = {storage}}, nullptr);
            if (!res) {
                return res;
            }
            if (nbit_typ == 8 && !sign) {
                u8_typ = *res;
            }
            return *res;
        };
    }

    auto loop(auto&& op, Varint cmp_id, Varint counter, auto&& body) -> Error {
        op(AbstractOp::LOOP_CONDITION, [&](Code& m) {
            m.ref(cmp_id);
        });
        BM_ERROR_WRAP_ERROR(error, body());
        BM_REF(op, AbstractOp::INC, counter);
        op(AbstractOp::END_LOOP, [&](Code& m) {});
        return none;
    }

}  // namespace rebgn