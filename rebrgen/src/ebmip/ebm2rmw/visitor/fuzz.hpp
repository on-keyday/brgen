/*license*/
#pragma once

#include "../codegen.hpp"
#include "interpret.hpp"
#include "layout.hpp"
#include <chrono>
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

        // Geometric-ish distribution: biased toward small values, bounded by max
        size_t next_vector_length(size_t max) {
            if (max == 0) return 0;
            size_t len = 0;
            while (len < max && (rng() % 3) != 0) {
                ++len;
            }
            return len;
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

    // Two-pass struct fill.
    // Pass 1 fills non-STRUCT_UNION fields (including discriminant fields).
    // Pass 2 uses the compiled selector to determine which STRUCT_UNION variant is
    // active, then fills only that variant's data.
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

        // Pass 1: non-STRUCT_UNION fields (populates discriminants)
        for (const auto& fl : struct_layout->fields) {
            if (ctx.get_kind(fl.type.type) == ebm::TypeKind::STRUCT_UNION) continue;
            if (fl.offset + fl.type.size > struct_obj.raw_object.size()) continue;
            fuzz_fill_object(ctx, rng, runtime,
                             ObjectRef(fl.type.type,
                                      struct_obj.raw_object.substr(fl.offset, fl.type.size)),
                             max_vec_len, access);
        }

        // Pass 2: STRUCT_UNION fields — use selector if available
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

        // Collect distinct field sizes for boundary tokens
        std::set<size_t> field_sizes;

        // Walk all statements looking for ENUM_MEMBER_DECL
        auto& stmts = ctx.module().module().statements;
        for (size_t i = 0; i < stmts.size(); i++) {
            auto& stmt = stmts[i];
            if (stmt.body.kind != ebm::StatementKind::ENUM_MEMBER_DECL) continue;
            auto* member = stmt.body.enum_member_decl();
            if (!member) continue;

            // Get the enum member's integer value
            auto* expr = ctx.module().get_expression(member->value);
            if (!expr) continue;
            const auto* int_val = expr->body.int_value();
            if (!int_val) continue;
            std::uint64_t val = int_val->value();

            // Get the base type size from the parent enum
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

            // Emit both little-endian and big-endian representations
            auto name = ctx.identifier(stmt.id);
            auto emit = [&](std::string_view suffix, auto write_fn) {
                buf += "# ";
                buf += name;
                buf += suffix;
                buf += "\n";
                buf += "\"";
                for (size_t b = 0; b < byte_size; b++) {
                    auto byte_val = write_fn(val, b);
                    char hex[5];
                    snprintf(hex, sizeof(hex), "\\x%02x", static_cast<unsigned>(byte_val));
                    buf += hex;
                }
                buf += "\"\n";
            };

            emit("_le", [](std::uint64_t v, size_t b) -> std::uint8_t {
                return static_cast<std::uint8_t>((v >> (b * 8)) & 0xff);
            });
            emit("_be", [&](std::uint64_t v, size_t b) -> std::uint8_t {
                return static_cast<std::uint8_t>((v >> ((byte_size - 1 - b) * 8)) & 0xff);
            });
            buf += "\n";
        }

        // Collect field sizes from the root struct
        std::vector<MutableField> fields;
        collect_mutable_fields(ctx, access, root_type, 0, "", fields);
        for (auto& f : fields) {
            field_sizes.insert(f.size);
        }

        // Emit boundary values for each field size
        buf += "# Boundary values by field size\n";
        for (size_t sz : field_sizes) {
            if (sz == 0 || sz > 8) continue;
            const size_t bits = sz * 8;
            const std::uint64_t max_val = (bits >= 64) ? ~std::uint64_t(0)
                                                        : ((std::uint64_t(1) << bits) - 1);
            // All zeros
            buf += std::format("# zeros_{}\n\"", sz);
            for (size_t b = 0; b < sz; b++) buf += "\\x00";
            buf += "\"\n";

            // All ones (max unsigned)
            buf += std::format("# max_{}\n\"", sz);
            for (size_t b = 0; b < sz; b++) {
                char hex[5];
                snprintf(hex, sizeof(hex), "\\x%02x",
                         static_cast<unsigned>((max_val >> (b * 8)) & 0xff));
                buf += hex;
            }
            buf += "\"\n";

            // 0x80... (sign bit / midpoint)
            if (sz >= 1) {
                const std::uint64_t mid = std::uint64_t(1) << (bits - 1);
                buf += std::format("# mid_{}_le\n\"", sz);
                for (size_t b = 0; b < sz; b++) {
                    char hex[5];
                    snprintf(hex, sizeof(hex), "\\x%02x",
                             static_cast<unsigned>((mid >> (b * 8)) & 0xff));
                    buf += hex;
                }
                buf += "\"\n";
            }
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
