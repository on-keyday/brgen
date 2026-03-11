/*license*/
#pragma once

#define HANDLE_ERROR(handle_error, expr) handle_error("{}\n at {} {}:{}", expr.error(), __func__, __FILE__, __LINE__)

#define BM_ERROR_WRAP(name, handle_error, expr)                     \
    auto tmp__##name##__ = expr;                                    \
    if (!tmp__##name##__) {                                         \
        return HANDLE_ERROR(handle_error, tmp__##name##__.error()); \
    }                                                               \
    auto name = std::move(*tmp__##name##__)

#define BM_ERROR_WRAP_ERROR(handle_error, expr)             \
    auto err__##name##__ = expr;                            \
    if (err__##name##__) {                                  \
        return HANDLE_ERROR(handle_error, err__##name##__); \
    }

#define BM_NEW_ID(name, handle_error, loc) \
    BM_ERROR_WRAP(name, handle_error, (m.new_id(loc)))

#define BM_NEW_NODE_ID(name, handle_error, node) \
    BM_ERROR_WRAP(name, handle_error, (m.new_node_id(node)))

#define BM_BINARY_BASE(op, id_name, bin_op, left, right, new_id, ...) \
    new_id(id_name __VA_OPT__(, )##__VA_ARGS__);                      \
    op(AbstractOp::BINARY, [&](Code& c) {                             \
        c.ident(id_name);                                             \
        c.bop(bin_op);                                                \
        c.left_ref(left);                                             \
        c.right_ref(right);                                           \
    })

#define BM_BINARY(op, id_name, bin_op, left, right) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_ID, error, nullptr)

#define BM_BINARY_NODE(op, id_name, bin_op, left, right, node) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_NODE_ID, error, node)

#define BM_BINARY_UNEXPECTED(op, id_name, bin_op, left, right) \
    BM_BINARY_BASE(op, id_name, bin_op, left, right, BM_NEW_ID, unexpect_error, nullptr)

#define BM_UNARY(op, id_name, un_op, ref_) \
    BM_NEW_ID(id_name, error, nullptr);    \
    op(AbstractOp::UNARY, [&](Code& c) {   \
        c.ident(id_name);                  \
        c.uop(un_op);                      \
        c.ref(ref_);                       \
    })

#define BM_GET_EXPR(target, m, node) \
    BM_ERROR_WRAP(target, error, (get_expr(m, node)))

#define BM_LOOKUP_IDENT(ident, m, node) \
    BM_ERROR_WRAP(ident, error, (m.lookup_ident(node)))

#define BM_DEFINE_STORAGE(ident, m, type_node, ...) \
    BM_ERROR_WRAP(ident, error, (define_storage(m, type_node __VA_OPT__(, )##__VA_ARGS__)))

#define BM_NEW_OBJECT(op, id_name, type_)     \
    BM_NEW_ID(id_name, error, nullptr);       \
    op(AbstractOp::NEW_OBJECT, [&](Code& c) { \
        c.ident(id_name);                     \
        c.type(type_);                        \
    })

#define BM_INDEX(op, id_name, array, index) \
    BM_NEW_ID(id_name, error, nullptr);     \
    op(AbstractOp::INDEX, [&](Code& c) {    \
        c.ident(id_name);                   \
        c.left_ref(array);                  \
        c.right_ref(index);                 \
    })

#define BM_IMMEDIATE(op, id_name, value) \
    BM_ERROR_WRAP(id_name, error, (immediate(m, op, value)))

#define BM_IMMEDIATE_UNEXPECT(op, id_name, value) \
    BM_ERROR_WRAP(id_name, unexpect_error, (immediate(m, op, value)))

#define BM_ASSIGN(op, id_name, left, right, prev_assign, loc) \
    BM_NEW_ID(id_name, error, loc);                           \
    op(AbstractOp::ASSIGN, [&](Code& c) {                     \
        c.ident(id_name);                                     \
        c.left_ref(left);                                     \
        c.right_ref(right);                                   \
        c.ref(prev_assign);                                   \
    })

#define BM_ASSIGN_NODE(op, id_name, left, right, prev_assign, node) \
    BM_NEW_NODE_ID(id_name, error, node);                           \
    op(AbstractOp::ASSIGN, [&](Code& c) {                           \
        c.ident(id_name);                                           \
        c.left_ref(left);                                           \
        c.right_ref(right);                                         \
        c.ref(prev_assign);                                         \
    })

#define BM_CAST_WITH_ERROR(op, id_name, error, dst_type, src_type, expr, cast_kind) \
    BM_NEW_ID(id_name, error, nullptr);                                             \
    op(AbstractOp::CAST, [&](Code& c__) {                                           \
        c__.ident(id_name);                                                         \
        c__.type(dst_type);                                                         \
        c__.from_type(src_type);                                                    \
        c__.ref(expr);                                                              \
        c__.cast_type(cast_kind);                                                   \
    })

#define BM_CAST(op, id_name, dst_type, src_type, expr, cast_kind) \
    BM_CAST_WITH_ERROR(op, id_name, error, dst_type, src_type, expr, cast_kind)

#define BM_REF(op, abs_op, ref_) op(abs_op, [&](Code& c) { c.ref(ref_); })
#define BM_OP(op, abs_op) op(abs_op, [&](Code& c) {})

#define BM_GET_STORAGE_REF_WITH_LOC(storage_ref, handle_error, storage, loc) \
    BM_ERROR_WRAP(storage_ref, handle_error, (m.get_storage_ref(storage, loc)))

#define BM_GET_STORAGE_REF(storage_ref, storage) \
    BM_GET_STORAGE_REF_WITH_LOC(storage_ref, error, storage, nullptr)

#define BM_ENCODE_INT(op, target, endian_, bit_size_, belong_) \
    BM_ERROR_WRAP(tmp_bit_size__, error, (varint(bit_size_))); \
    op(AbstractOp::ENCODE_INT, [&](Code& c__) {                \
        c__.ref(target);                                       \
        c__.endian(endian_);                                   \
        c__.bit_size(tmp_bit_size__);                          \
        c__.belong(belong_);                                   \
    });

#define BM_DECODE_INT(op, target, endian_, bit_size_, belong_) \
    BM_ERROR_WRAP(tmp_bit_size__, error, (varint(bit_size_))); \
    op(AbstractOp::DECODE_INT, [&](Code& c__) {                \
        c__.ref(target);                                       \
        c__.endian(endian_);                                   \
        c__.bit_size(tmp_bit_size__);                          \
        c__.belong(belong_);                                   \
    });

#define BM_ENCODE_INT_VEC_FIXED(op, target, len, endian_, bit_size_, belong_, array_length_) \
    BM_ERROR_WRAP(tmp_bit_size__, error, (varint(bit_size_)));                               \
    BM_ERROR_WRAP(tmp_array_length__, error, (varint(array_length_)));                       \
    op(AbstractOp::ENCODE_INT_VECTOR_FIXED, [&](Code& c__) {                                 \
        c__.left_ref(target);                                                                \
        c__.right_ref(len);                                                                  \
        c__.endian(endian_);                                                                 \
        c__.bit_size(tmp_bit_size__);                                                        \
        c__.belong(belong_);                                                                 \
        c__.array_length(tmp_array_length__);                                                \
    });

#define BM_DECODE_INT_VEC_FIXED(op, target, len, endian_, bit_size_, belong_, array_length_) \
    BM_ERROR_WRAP(tmp_bit_size__, error, (varint(bit_size_)));                               \
    BM_ERROR_WRAP(tmp_array_length__, error, (varint(array_length_)));                       \
    op(AbstractOp::DECODE_INT_VECTOR_FIXED, [&](Code& c__) {                                 \
        c__.left_ref(target);                                                                \
        c__.right_ref(len);                                                                  \
        c__.endian(endian_);                                                                 \
        c__.bit_size(tmp_bit_size__);                                                        \
        c__.belong(belong_);                                                                 \
        c__.array_length(tmp_array_length__);                                                \
    });

#define BM_GET_ENDIAN(id_name, endian, is_signed) \
    BM_ERROR_WRAP(id_name, error, (m.get_endian(Endian(endian), is_signed)))

#define BM_BEGIN_COND_BLOCK(op, base_codes, cond_block, loc) \
    BM_NEW_ID(cond_block, error, loc);                       \
    op(AbstractOp::BEGIN_COND_BLOCK, [&](Code& c) {          \
        c.ident(cond_block);                                 \
    });                                                      \
    auto tmp_index__##cond_block##__ = base_codes.size() - 1;

#define BM_END_COND_BLOCK(op, base_codes, cond_block, cond) \
    base_codes[tmp_index__##cond_block##__].ref(cond);      \
    op(AbstractOp::END_COND_BLOCK);

#define BM_COND_IN_BLOCK(op, base_codes, cond_block, cond)       \
    BM_BEGIN_COND_BLOCK(op, base_codes, cond_block, &cond->loc); \
    BM_GET_EXPR(cond_expr##cond_block, m, cond);                 \
    BM_END_COND_BLOCK(op, base_codes, cond_block, cond_expr##cond_block);
