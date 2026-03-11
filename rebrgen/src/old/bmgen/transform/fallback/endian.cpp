/*license*/
#include <bmgen/fallback.hpp>

namespace rebgn {

    Error apply_dynamic_endian_fallback(Module& m) {
        std::vector<Code> new_code;
        NEW_CODE_OP(new_code);
        std::optional<StorageRef> u8_typ;
        auto get_nbit_typ = nbit_type_getter(m, u8_typ);
        std::unordered_map<ObjectID, Varint> mapping;
        for (auto& code : m.code) {
            if (code.op == AbstractOp::DYNAMIC_ENDIAN) {
                BM_NEW_ID(fallback_id, error, nullptr);
                code.fallback(fallback_id);
                op(AbstractOp::DEFINE_FALLBACK, [&](Code& m) {
                    m.ident(fallback_id);
                });
                BM_GET_STORAGE_REF(bool_typ, (Storages{.length = *varint(1), .storages = {Storage{.type = StorageType::BOOL}}}));
                BM_ERROR_WRAP(endian_expr_typ, error, (get_nbit_typ(8, false)));
                BM_ERROR_WRAP(is_little_endian, error, (new_default_var(bool_typ)));
                BM_ERROR_WRAP(endian_expr_var, error, (new_default_var(endian_expr_typ)));
                // if result is 1, then little endian
                // else if 2, then native endian,
                // otherwise (usually, 0) big endian
                auto endian_expr = code.ref().value();
                BM_ASSIGN(op, endian_assign_id, endian_expr, endian_expr_var, null_varint, nullptr);

                BM_ERROR_WRAP(immTrue, error, immediate_bool(m, op, true));

                BM_IMMEDIATE(op, imm1, 1);
                BM_IMMEDIATE(op, imm2, 2);

                BM_BINARY(op, cmp1, BinaryOp::equal, endian_expr, imm1);
                BM_REF(op, AbstractOp::IF, cmp1);
                BM_ASSIGN(op, assign1, is_little_endian, immTrue, null_varint, nullptr);

                BM_BINARY(op, cmp2, BinaryOp::equal, endian_expr, imm2);
                BM_REF(op, AbstractOp::ELIF, cmp2);
                BM_NEW_ID(native_endian, error, nullptr);
                op(AbstractOp::IS_LITTLE_ENDIAN, [&](Code& m) {
                    m.ident(native_endian);
                });
                BM_ASSIGN(op, assign2, is_little_endian, native_endian, null_varint, nullptr);

                BM_OP(op, AbstractOp::END_IF);  // no need to change variable if big endian

                op(AbstractOp::END_FALLBACK, [&](Code& m) {});

                mapping[code.ident()->value()] = is_little_endian;
            }
            else if (code.op == AbstractOp::IS_LITTLE_ENDIAN) {
                auto ref = code.ref().value();
                if (ref.value() == 0) {
                    continue;  // this is native endian, skip
                }
                auto found = mapping.find(ref.value());
                if (found == mapping.end()) {
                    return error("Invalid endian reference");
                }
                code.fallback(found->second);
            }
        }
        m.code.insert_range(m.code.end(), std::move(new_code));
        return none;
    }
}  // namespace rebgn
