/*license*/
#pragma once

#include "../codegen.hpp"
#include "interpret.hpp"
#include "layout.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <random>
#include <set>

namespace ebm2rmw {

    // Simple PRNG wrapper providing type-appropriate random values.
    struct FuzzRng {
       private:
        std::mt19937_64 rng;

        // Interesting boundary values used with ~12% probability for integer fields
        static constexpr std::uint64_t interesting_values[] = {
            0, 1, 2,
            0x7fULL, 0x80ULL, 0xffULL,
            0x100ULL, 0x7fffULL, 0x8000ULL, 0xffffULL,
            0x10000ULL, 0x7fffffffULL, 0x80000000ULL, 0xffffffffULL,
            0x100000000ULL, 0x7fffffffffffffffULL, 0xffffffffffffffffULL,
        };

       public:
        explicit FuzzRng(std::uint64_t seed)
            : rng(seed) {}

        std::uint64_t next_raw() {
            return rng();
        }

        // Random unsigned integer bounded to [0, 2^bits)
        std::uint64_t next_uint(size_t bits) {
            if (bits == 0) return 0;
            const std::uint64_t mask = (bits >= 64) ? ~std::uint64_t(0)
                                                     : ((std::uint64_t(1) << bits) - 1);
            // ~12% chance of an interesting boundary value
            if ((rng() & 7) == 0) {
                const size_t idx = static_cast<size_t>(rng() % std::size(interesting_values));
                return interesting_values[idx] & mask;
            }
            return rng() & mask;
        }

        bool next_bool() {
            return (rng() & 1) != 0;
        }

        size_t next_choice(size_t n) {
            if (n <= 1) return 0;
            return static_cast<size_t>(rng() % n);
        }

        // Log-uniform distribution: covers [0, max] with bias toward smaller values
        // but still produces large values regularly.
        // ~12% chance of a boundary value (0, 1, max/2, max).
        size_t next_vector_length(size_t max) {
            if (max == 0) return 0;
            if ((rng() & 7) == 0) {
                // Boundary values
                const size_t boundaries[] = {0, 1, max / 2, max};
                return boundaries[rng() % 4];
            }
            // Log-uniform: uniform in log space → exponential spread
            // exp(uniform(0, log(max+1))) - 1, but using integer math
            double log_max = std::log(static_cast<double>(max + 1));
            double u = static_cast<double>(rng() & 0xFFFFFFFF) / static_cast<double>(0xFFFFFFFF);
            auto result = static_cast<size_t>(std::exp(u * log_max));
            return std::min(result, max);
        }
    };

    // Forward declarations
    void fuzz_fill_object(InitialContext& ctx, FuzzRng& rng, RuntimeEnv& runtime,
                          ObjectRef obj, size_t max_vec_len, LayoutAccess& access);
    void fuzz_fill_struct(InitialContext& ctx, FuzzRng& rng, RuntimeEnv& runtime,
                          ObjectRef struct_obj, size_t max_vec_len, LayoutAccess& access);

    // Fill a field/object with random data appropriate for its type.
    // On any error the field is left at whatever value it already had (typically zero).
    // This is intentional: partial fills still exercise interesting parser behaviour.
    inline void fuzz_fill_object(InitialContext& ctx, FuzzRng& rng, RuntimeEnv& runtime,
                                 ObjectRef obj, size_t max_vec_len, LayoutAccess& access) {
        if (obj.raw_object.empty()) return;

        const auto opt_kind = ctx.get_kind(obj.type);
        if (!opt_kind) return;
        switch (*opt_kind) {
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::INT: {
                const size_t bits = obj.raw_object.size() * 8;
                const std::uint64_t val = rng.next_uint(bits);
                if (!encode_uint64(obj, val)) {
                    // Happens only when size > 8 bytes; fill byte-by-byte instead
                    for (auto& b : obj.raw_object) {
                        b = static_cast<std::uint8_t>(rng.next_uint(8));
                    }
                }
                break;
            }
            case ebm::TypeKind::BOOL: {
                encode_uint64(obj, rng.next_bool() ? 1 : 0);
                break;
            }
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::FLOAT: {
                // Raw byte fill: encode handles semantic constraints
                for (auto& b : obj.raw_object) {
                    b = static_cast<std::uint8_t>(rng.next_uint(8));
                }
                break;
            }
            case ebm::TypeKind::ARRAY: {
                auto elem_res = access.get_array_element_type(obj.type);
                if (!elem_res) break;  // layout missing: leave as zero
                const auto& elem_layout = *elem_res;
                if (elem_layout.size == 0) break;
                const size_t count = obj.raw_object.size() / elem_layout.size;
                for (size_t i = 0; i < count; i++) {
                    ObjectRef elem_obj(elem_layout.type,
                                      obj.raw_object.substr(i * elem_layout.size, elem_layout.size));
                    fuzz_fill_object(ctx, rng, runtime, elem_obj, max_vec_len, access);
                }
                break;
            }
            case ebm::TypeKind::VECTOR: {
                auto elem_res = access.get_vector_element_type(obj.type);
                if (!elem_res) break;  // layout missing: leave as zero
                const auto& elem_layout = *elem_res;
                if (elem_layout.size == 0) break;
                const size_t count = rng.next_vector_length(max_vec_len);
                if (count == 0) {
                    runtime.vector_init(obj);  // ensure arena entry exists even for empty vectors
                    break;
                }
                for (size_t i = 0; i < count; i++) {
                    auto new_elem_res = runtime.vector_alloc_back(
                        ctx, obj, elem_layout.size, elem_layout.type, nullptr);
                    if (!new_elem_res) break;  // arena error: leave vector shorter
                    fuzz_fill_object(ctx, rng, runtime, *new_elem_res, max_vec_len, access);
                }
                break;
            }
            case ebm::TypeKind::STRUCT: {
                fuzz_fill_struct(ctx, rng, runtime, obj, max_vec_len, access);
                break;
            }
            case ebm::TypeKind::VARIANT:
            case ebm::TypeKind::STRUCT_UNION: {
                // Pick a random variant. For STRUCT_UNION called directly (not via
                // fuzz_fill_struct) we lack the parent needed for selector evaluation.
                auto* union_layout = access.get_union_layout(obj.type);
                if (!union_layout || union_layout->variants.empty()) break;
                const size_t variant_idx = rng.next_choice(union_layout->variants.size());
                const auto& variant = union_layout->variants[variant_idx];
                if (variant.size == 0 || variant.size > obj.raw_object.size()) break;
                fuzz_fill_object(ctx, rng, runtime,
                                 ObjectRef(variant.type, obj.raw_object.substr(0, variant.size)),
                                 max_vec_len, access);
                break;
            }
            default:
                // RECURSIVE_STRUCT, OPTIONAL, etc.: leave as zero (safe)
                break;
        }
    }

    // =========================================================================
    // length_expr field resolver: walk the expression tree to find which
    // struct field(s) a VECTOR's length_expr references, so we can write
    // back a value that makes the encoder happy.
    // =========================================================================

    // Info about a field referenced by length_expr
    struct LengthFieldInfo {
        size_t offset = 0;   // byte offset within the struct
        size_t size = 0;     // byte size of the field
    };

    // Recursively walk an expression tree rooted at `expr_ref` to find
    // the first MEMBER_ACCESS → IDENTIFIER → FIELD_DECL leaf that refers
    // to a scalar field in the struct.  Returns the field's byte offset
    // and size inside `struct_obj`, or nullopt if the pattern is too complex.
    inline std::optional<LengthFieldInfo> find_length_field(
        InitialContext& ctx, LayoutAccess& access,
        ebm::ExpressionRef expr_ref, ebm::TypeRef struct_type) {
        auto* expr = ctx.module().get_expression(expr_ref);
        if (!expr) return std::nullopt;

        switch (expr->body.kind) {
            case ebm::ExpressionKind::MEMBER_ACCESS: {
                // Check if the base is SELF (direct field ref like self.length)
                auto* base_ptr = expr->body.base();
                auto* member_ptr = expr->body.member();
                if (!base_ptr || !member_ptr) return std::nullopt;

                auto* base_expr = ctx.module().get_expression(*base_ptr);
                if (!base_expr) return std::nullopt;

                if (base_expr->body.kind == ebm::ExpressionKind::SELF) {
                    // Direct field: self.field_name
                    auto* member_expr = ctx.module().get_expression(*member_ptr);
                    if (!member_expr) return std::nullopt;
                    auto* field_id = member_expr->body.id();
                    if (!field_id) return std::nullopt;
                    auto field_ref = from_weak(*field_id);

                    // Look up field in the struct layout
                    auto* sl = access.get_struct_layout_detail(struct_type);
                    if (!sl) return std::nullopt;
                    for (const auto& fl : sl->fields) {
                        if (fl.field == field_ref) {
                            return LengthFieldInfo{fl.offset, fl.type.size};
                        }
                    }
                    return std::nullopt;
                }

                if (base_expr->body.kind == ebm::ExpressionKind::MEMBER_ACCESS) {
                    // Nested: self.header.length — recurse on base to get
                    // the outer struct field, then resolve the inner field
                    auto* member_expr = ctx.module().get_expression(*member_ptr);
                    if (!member_expr) return std::nullopt;
                    auto* inner_field_id = member_expr->body.id();
                    if (!inner_field_id) return std::nullopt;
                    auto inner_field_ref = from_weak(*inner_field_id);

                    // Get the base field info (e.g., self.header)
                    auto base_info = find_length_field(ctx, access, *base_ptr, struct_type);
                    if (!base_info) return std::nullopt;

                    // Now resolve the inner field's offset within the nested struct.
                    // We need the type of the base expression to find the inner layout.
                    auto base_type = base_expr->body.type;
                    auto* inner_sl = access.get_struct_layout_detail(base_type);
                    if (!inner_sl) return std::nullopt;
                    for (const auto& fl : inner_sl->fields) {
                        if (fl.field == inner_field_ref) {
                            return LengthFieldInfo{
                                base_info->offset + fl.offset,
                                fl.type.size};
                        }
                    }
                    return std::nullopt;
                }
                return std::nullopt;
            }
            case ebm::ExpressionKind::BINARY_OP: {
                // e.g. self.length - 8 → recurse on left and right to find the field
                auto* left = expr->body.left();
                auto* right = expr->body.right();
                if (left) {
                    auto res = find_length_field(ctx, access, *left, struct_type);
                    if (res) return res;
                }
                if (right) {
                    auto res = find_length_field(ctx, access, *right, struct_type);
                    if (res) return res;
                }
                return std::nullopt;
            }
            case ebm::ExpressionKind::TYPE_CAST: {
                auto* desc = expr->body.type_cast_desc();
                if (!desc) return std::nullopt;
                return find_length_field(ctx, access, desc->source_expr, struct_type);
            }
            default:
                return std::nullopt;
        }
    }

    // After choosing a target vector count and filling the VECTOR, adjust the
    // length field so that the encoder's length_expr check passes.
    //
    // Strategy:
    //   1. find_length_field() walks the expression tree to locate the scalar
    //      field (byte offset + size) that drives the vector length.
    //   2. Binary-search: try candidate values for that field, evaluate
    //      length_expr via the compiled function, pick the value whose result
    //      equals the target count.
    //   3. If binary search fails within a few iterations, try a linear
    //      scan near the target for small fields.
    inline void fuzz_adjust_length_field(
        InitialContext& ctx, RuntimeEnv& runtime,
        ObjectRef struct_obj, ebm::TypeRef vec_type,
        ebm::StatementRef length_fn, size_t target_count,
        LayoutAccess& access) {
        ebm::ExpressionRef length_expr{};
        if (auto* type_entry = ctx.module().get_type(vec_type)) {
            if (auto* le = type_entry->body.length_expr()) {
                length_expr = *le;
            }
        }
        auto field_info = find_length_field(ctx, access, length_expr, struct_obj.type);
        if (!field_info || field_info->size == 0 || field_info->size > 8) return;

        // Helper: write a value to the length field bytes and evaluate
        auto try_value = [&](std::uint64_t val) -> std::optional<std::uint64_t> {
            auto field_bytes = struct_obj.raw_object.substr(field_info->offset, field_info->size);
            // Write LE bytes (the interpreter reads them this way)
            for (size_t i = 0; i < field_info->size; i++) {
                field_bytes[i] = static_cast<std::uint8_t>((val >> (i * 8)) & 0xff);
            }
            auto res = runtime.eval_vector_length(ctx, length_fn, struct_obj);
            if (!res) return std::nullopt;
            return *res;
        };

        const std::uint64_t max_field_val = (field_info->size >= 8)
            ? ~std::uint64_t(0)
            : (std::uint64_t(1) << (field_info->size * 8)) - 1;

        // Try simple linear probe: target_count, target_count+1, ...
        // (covers identity case: field == count)
        // and target_count + small_offset (covers field - constant case)
        for (std::uint64_t probe = target_count; probe <= std::min(static_cast<std::uint64_t>(target_count) + 256, max_field_val); probe++) {
            auto result = try_value(probe);
            if (result && *result == target_count) return;  // found it
        }
        // Also try below target (in case of multiplication or other patterns)
        for (std::uint64_t probe = (target_count > 0 ? target_count - 1 : 0);
             probe > 0 && probe >= (target_count > 256 ? target_count - 256 : 0); probe--) {
            auto result = try_value(probe);
            if (result && *result == target_count) return;
        }
        // Give up — leave current value; encode may fail but fuzzer continues
    }

    // Three-pass struct fill.
    // Pass 1: Fill non-VECTOR, non-STRUCT_UNION fields (populates discriminants, length fields).
    // Pass 2: Fill VECTOR fields using compiled length_expr (if available) to determine
    //         the element count from the already-filled length field value.
    // Pass 3: Fill STRUCT_UNION fields using compiled selector function.
    inline void fuzz_fill_struct(InitialContext& ctx, FuzzRng& rng, RuntimeEnv& runtime,
                                 ObjectRef struct_obj, size_t max_vec_len, LayoutAccess& access) {
        auto* struct_layout = access.get_struct_layout_detail(struct_obj.type);
        if (!struct_layout) {
            // Layout unknown: raw byte fill as fallback
            for (auto& b : struct_obj.raw_object) {
                b = static_cast<std::uint8_t>(rng.next_uint(8));
            }
            return;
        }

        // Pass 1: non-VECTOR, non-STRUCT_UNION fields (populates discriminants and length fields)
        for (const auto& fl : struct_layout->fields) {
            const auto kind = ctx.get_kind(fl.type.type);
            if (kind == ebm::TypeKind::STRUCT_UNION || kind == ebm::TypeKind::VECTOR) continue;
            if (fl.offset + fl.type.size > struct_obj.raw_object.size()) continue;
            fuzz_fill_object(ctx, rng, runtime,
                             ObjectRef(fl.type.type,
                                      struct_obj.raw_object.substr(fl.offset, fl.type.size)),
                             max_vec_len, access);
        }

        // Pass 2: VECTOR fields — determine element count, then fill.
        // Strategy: pick a random count (bounded by max_vec_len), fill the vector,
        // then use a binary-search approach to find the length field value that makes
        // the compiled length_expr evaluate to exactly that count. Write it back.
        for (const auto& fl : struct_layout->fields) {
            if (ctx.get_kind(fl.type.type) != ebm::TypeKind::VECTOR) continue;
            if (fl.offset + fl.type.size > struct_obj.raw_object.size()) continue;

            auto elem_res = access.get_vector_element_type(fl.type.type);
            if (!elem_res) continue;
            const auto& elem_layout = *elem_res;
            if (elem_layout.size == 0) continue;

            ObjectRef field_obj(fl.type.type,
                                struct_obj.raw_object.substr(fl.offset, fl.type.size));

            // Choose a random vector count
            size_t count = rng.next_vector_length(max_vec_len);

            // If we have a compiled length_expr, adjust the length field so the
            // encoder's check (length_expr == vector.size()) passes.
            // Skip adjustment when --skip-write-size-check is set — intentionally
            // produce mismatched length/data for fuzzing decoders.
            if (!ctx.flags().skip_write_size_check) {
                auto* length_fn = access.get_vector_length_fn(fl.type.type);
                if (length_fn) {
                    fuzz_adjust_length_field(ctx, runtime, struct_obj, fl.type.type,
                                              *length_fn, count, access);
                }
            }

            if (count == 0) {
                runtime.vector_init(field_obj);  // ensure arena entry exists
            } else {
                for (size_t i = 0; i < count; i++) {
                    auto new_elem_res = runtime.vector_alloc_back(
                        ctx, field_obj, elem_layout.size, elem_layout.type, nullptr);
                    if (!new_elem_res) break;
                    fuzz_fill_object(ctx, rng, runtime, *new_elem_res, max_vec_len, access);
                }
            }
        }

        // Pass 3: STRUCT_UNION fields — use selector if available
        for (const auto& fl : struct_layout->fields) {
            if (ctx.get_kind(fl.type.type) != ebm::TypeKind::STRUCT_UNION) continue;
            if (fl.offset + fl.type.size > struct_obj.raw_object.size()) continue;
            ObjectRef field_obj(fl.type.type,
                                struct_obj.raw_object.substr(fl.offset, fl.type.size));

            auto* union_layout = access.get_union_layout(fl.type.type);
            if (!union_layout || union_layout->variants.empty()) continue;

            size_t variant_idx = 0;
            const auto* selector_ref = access.get_struct_union_selector_fn(fl.type.type);
            if (selector_ref) {
                auto vres = runtime.eval_struct_union_variant(ctx, *selector_ref, struct_obj);
                if (vres && *vres < union_layout->variants.size()) {
                    variant_idx = *vres;
                }
                // Selector failed or returned OOB: use variant 0
            } else {
                variant_idx = rng.next_choice(union_layout->variants.size());
            }

            const auto& variant = union_layout->variants[variant_idx];
            if (variant.size == 0 || variant.size > field_obj.raw_object.size()) continue;
            fuzz_fill_object(ctx, rng, runtime,
                             ObjectRef(variant.type, field_obj.raw_object.substr(0, variant.size)),
                             max_vec_len, access);
        }
    }

    // =========================================================================
    // Common fuzz helpers
    // =========================================================================

    inline std::uint64_t resolve_fuzz_seed(std::uint64_t seed) {
        if (seed == 0) {
            seed = static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
        }
        return seed;
    }

    inline expected<std::filesystem::path> prepare_corpus_dir(std::string_view dir_flag) {
        if (dir_flag.empty()) return std::filesystem::path{};
        std::filesystem::path corpus_dir{dir_flag};
        std::error_code ec;
        std::filesystem::create_directories(corpus_dir, ec);
        if (ec) {
            return ebmgen::unexpect_error("Failed to create corpus dir '{}': {}",
                                           dir_flag, ec.message());
        }
        return corpus_dir;
    }

    // =========================================================================
    // Mutation support
    // =========================================================================

    // A mutable leaf field: its path (for diagnostics) and how to locate it
    // within a decoded object's byte buffer.
    struct MutableField {
        std::string path;       // e.g. "header.src_port"
        size_t offset;          // byte offset within decoded_self_bytes
        size_t size;            // byte size of the field
        ebm::TypeRef type;      // EBM type (for kind lookup)
    };

    // Recursively collect all leaf (scalar) fields reachable from a struct.
    // Arrays are expanded element-by-element; vectors and unions are skipped
    // because their storage is arena-based (not contiguous in decoded_self_bytes).
    inline void collect_mutable_fields(InitialContext& ctx,
                                       LayoutAccess& access,
                                       ebm::TypeRef type,
                                       size_t base_offset,
                                       const std::string& path_prefix,
                                       std::vector<MutableField>& out) {
        const auto opt_kind = ctx.get_kind(type);
        if (!opt_kind) return;
        switch (*opt_kind) {
            case ebm::TypeKind::UINT:
            case ebm::TypeKind::INT:
            case ebm::TypeKind::BOOL:
            case ebm::TypeKind::ENUM:
            case ebm::TypeKind::FLOAT:
                // Leaf scalar — record it
                if (auto tl = access.get_type_layout(type)) {
                    out.push_back({path_prefix, base_offset, tl->size, type});
                }
                break;
            case ebm::TypeKind::STRUCT: {
                auto* sl = access.get_struct_layout_detail(type);
                if (!sl) break;
                for (const auto& fl : sl->fields) {
                    auto id = ctx.identifier(fl.field);
                    std::string child_path = path_prefix.empty()
                                                 ? (id.empty() ? "?" : id)
                                                 : path_prefix + "." + (id.empty() ? "?" : id);
                    collect_mutable_fields(ctx, access, fl.type.type,
                                           base_offset + fl.offset, child_path, out);
                }
                break;
            }
            case ebm::TypeKind::ARRAY: {
                auto elem_res = access.get_array_element_type(type);
                if (!elem_res) break;
                auto tl = access.get_type_layout(type);
                if (!tl) break;
                const size_t count = tl->size / elem_res->size;
                for (size_t i = 0; i < count; i++) {
                    std::string child_path = path_prefix + "[" + std::to_string(i) + "]";
                    collect_mutable_fields(ctx, access, elem_res->type,
                                           base_offset + i * elem_res->size,
                                           child_path, out);
                }
                break;
            }
            // VECTOR, VARIANT, STRUCT_UNION: arena-based or selector-dependent,
            // skip for in-place mutation (generate mode handles these)
            default:
                break;
        }
    }

    // Mutation strategies applied to a single field's bytes within decoded_self_bytes.
    enum class MutationKind : int {
        BIT_FLIP = 0,       // flip a random bit
        BOUNDARY_VALUE,     // replace with an interesting boundary value
        RANDOM_REPLACE,     // replace with a completely random value
        BYTE_SWAP,          // swap two random bytes within the field
        INCREMENT,          // increment by a small random amount
        NUM_KINDS
    };

    inline void apply_mutation(FuzzRng& rng,
                               std::vector<std::uint8_t>& bytes,
                               const MutableField& field) {
        if (field.size == 0) return;
        const size_t end = field.offset + field.size;
        if (end > bytes.size()) return;

        const auto strategy = static_cast<MutationKind>(
            rng.next_choice(static_cast<size_t>(MutationKind::NUM_KINDS)));

        switch (strategy) {
            case MutationKind::BIT_FLIP: {
                const size_t bit_idx = static_cast<size_t>(rng.next_raw() % (field.size * 8));
                bytes[field.offset + bit_idx / 8] ^= static_cast<std::uint8_t>(1 << (bit_idx % 8));
                break;
            }
            case MutationKind::BOUNDARY_VALUE: {
                // Write an interesting value (truncated to field size)
                const std::uint64_t val = rng.next_uint(field.size * 8);
                for (size_t i = 0; i < field.size && i < 8; i++) {
                    bytes[field.offset + i] = static_cast<std::uint8_t>((val >> (i * 8)) & 0xff);
                }
                break;
            }
            case MutationKind::RANDOM_REPLACE: {
                for (size_t i = 0; i < field.size; i++) {
                    bytes[field.offset + i] = static_cast<std::uint8_t>(rng.next_uint(8));
                }
                break;
            }
            case MutationKind::BYTE_SWAP: {
                if (field.size >= 2) {
                    const size_t a = static_cast<size_t>(rng.next_raw() % field.size);
                    size_t b = static_cast<size_t>(rng.next_raw() % field.size);
                    if (b == a) b = (a + 1) % field.size;
                    std::swap(bytes[field.offset + a], bytes[field.offset + b]);
                }
                else {
                    // single byte: just flip a bit
                    bytes[field.offset] ^= static_cast<std::uint8_t>(1 << (rng.next_raw() % 8));
                }
                break;
            }
            case MutationKind::INCREMENT: {
                // Small increment/decrement (wrapping)
                const int delta = static_cast<int>(rng.next_raw() % 5) - 2;  // -2..+2
                if (field.size <= 8) {
                    std::uint64_t val = 0;
                    for (size_t i = 0; i < field.size; i++) {
                        val |= static_cast<std::uint64_t>(bytes[field.offset + i]) << (i * 8);
                    }
                    val = static_cast<std::uint64_t>(static_cast<std::int64_t>(val) + delta);
                    for (size_t i = 0; i < field.size; i++) {
                        bytes[field.offset + i] = static_cast<std::uint8_t>((val >> (i * 8)) & 0xff);
                    }
                }
                else {
                    bytes[field.offset] = static_cast<std::uint8_t>(bytes[field.offset] + delta);
                }
                break;
            }
            default:
                break;
        }
    }

    // =========================================================================
    // Dictionary generation for external fuzzers (AFL/libFuzzer format)
    // =========================================================================

    // Extract enum member values from the EBM module and write them as dictionary
    // entries.  Also emits boundary values for each distinct field size found.
    inline expected<void> generate_fuzz_dict(InitialContext& ctx,
                                              LayoutAccess& access,
                                              ebm::TypeRef root_type,
                                              std::string_view dict_path) {
        auto out = futils::file::File::create(dict_path);
        if (!out) {
            return ebmgen::unexpect_error("Failed to create dict file '{}': {}",
                                           dict_path, out.error().error<std::string>());
        }

        std::string buf;
        buf += "# Auto-generated fuzzer dictionary\n";
        buf += "# Format: AFL/libFuzzer compatible\n\n";

        auto append_hex_byte = [&](std::uint8_t byte_val) {
            buf += std::format("\\x{:02x}", byte_val);
        };

        auto append_hex_value = [&](std::uint64_t val, size_t byte_size, auto byte_order_fn) {
            buf += "\"";
            for (size_t b = 0; b < byte_size; b++) {
                append_hex_byte(byte_order_fn(val, b));
            }
            buf += "\"\n";
        };

        // Collect distinct field sizes for boundary tokens
        std::set<size_t> field_sizes;

        // Walk all statements looking for ENUM_MEMBER_DECL
        auto& stmts = ctx.module().module().statements;
        for (size_t i = 0; i < stmts.size(); i++) {
            auto& stmt = stmts[i];
            if (stmt.body.kind != ebm::StatementKind::ENUM_MEMBER_DECL) continue;
            auto* member = stmt.body.enum_member_decl();
            if (!member) continue;

            auto* expr = ctx.module().get_expression(member->value);
            if (!expr) continue;
            const auto* int_val = expr->body.int_value();
            if (!int_val) continue;
            std::uint64_t val = int_val->value();

            size_t byte_size = 0;
            auto* enum_stmt = ctx.module().get_statement(member->enum_decl);
            if (enum_stmt) {
                auto* edecl = enum_stmt->body.enum_decl();
                if (edecl) {
                    auto tl = access.get_type_layout(edecl->base_type);
                    if (tl) byte_size = tl->size;
                }
            }
            if (byte_size == 0) byte_size = (val <= 0xff) ? 1 : (val <= 0xffff) ? 2
                                                              : (val <= 0xffffffff) ? 4
                                                                                    : 8;

            auto name = ctx.identifier(stmt.id);
            auto le = [](std::uint64_t v, size_t b) -> std::uint8_t {
                return static_cast<std::uint8_t>((v >> (b * 8)) & 0xff);
            };
            auto be = [&](std::uint64_t v, size_t b) -> std::uint8_t {
                return static_cast<std::uint8_t>((v >> ((byte_size - 1 - b) * 8)) & 0xff);
            };

            buf += std::format("# {}_le\n", name);
            append_hex_value(val, byte_size, le);
            buf += std::format("# {}_be\n", name);
            append_hex_value(val, byte_size, be);
            buf += "\n";
        }

        // Collect field sizes from the root struct
        std::vector<MutableField> fields;
        collect_mutable_fields(ctx, access, root_type, 0, "", fields);
        for (auto& f : fields) {
            field_sizes.insert(f.size);
        }

        // Emit boundary values for each field size
        auto le = [](std::uint64_t v, size_t b) -> std::uint8_t {
            return static_cast<std::uint8_t>((v >> (b * 8)) & 0xff);
        };
        buf += "# Boundary values by field size\n";
        for (size_t sz : field_sizes) {
            if (sz == 0 || sz > 8) continue;
            const size_t bits = sz * 8;
            const std::uint64_t max_val = (bits >= 64) ? ~std::uint64_t(0)
                                                        : ((std::uint64_t(1) << bits) - 1);
            buf += std::format("# zeros_{}\n", sz);
            append_hex_value(0, sz, le);

            buf += std::format("# max_{}\n", sz);
            append_hex_value(max_val, sz, le);

            buf += std::format("# mid_{}_le\n", sz);
            append_hex_value(std::uint64_t(1) << (bits - 1), sz, le);
        }

        auto w = out->write_file_all(futils::view::rvec(buf));
        if (!w) {
            return ebmgen::unexpect_error("Failed to write dict file '{}': {}",
                                           dict_path, w.error().error<std::string>());
        }
        futils::wrap::cerr_wrap() << "Dictionary written to: " << dict_path
                                  << " (" << buf.size() << " bytes)\n";
        return {};
    }

}  // namespace ebm2rmw
