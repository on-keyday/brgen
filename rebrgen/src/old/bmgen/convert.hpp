/*license*/
#pragma once
#include <bm/binary_module.hpp>
#include <unordered_map>
#include <helper/expected.h>
#include <error/error.h>
#include <core/ast/ast.h>
#include <core/ast/tool/ident.h>
#include "common.hpp"
#include "helper.hpp"
#include <unordered_set>
namespace rebgn {

    using ObjectID = std::uint64_t;

    constexpr ObjectID null_id = 0;

    constexpr Varint null_varint = Varint();

    constexpr expected<Varint> varint(std::uint64_t n) {
        Varint v;
        if (n < 0x40) {
            v.prefix(0);
            v.value(n);
        }
        else if (n < 0x4000) {
            v.prefix(1);
            v.value(n);
        }
        else if (n < 0x40000000) {
            v.prefix(2);
            v.value(n);
        }
        else if (n < 0x4000000000000000) {
            v.prefix(3);
            v.value(n);
        }
        else {
            return unexpect_error("Invalid varint value: {}", n);
        }
        return v;
    }

    // Control Flow Graph
    // basic_block: list of Module.code index
    // next: list of next CFG
    // prev: list of previous CFG
    struct CFG {
        std::vector<size_t> basic_block;
        size_t sum_bits = 0;
        std::vector<std::shared_ptr<CFG>> next;
        std::vector<std::weak_ptr<CFG>> prev;
    };

    struct PhiCandidate {
        ObjectID condition;
        std::unordered_map<ObjectID, ObjectID> candidate;
    };

    struct PhiStack {
        std::unordered_map<ObjectID, ObjectID> start_state;
        ObjectID current_condition = null_id;
        std::vector<PhiCandidate> candidates;
        ObjectID current_dynamic_endian = null_id;
        Endian current_endian = Endian::unspec;
    };

    struct Module {
        std::unordered_map<std::string, ObjectID> metadata_table;
        std::unordered_map<ObjectID, std::string> metadata_table_rev;
        std::unordered_map<std::string, ObjectID> string_table;
        std::unordered_map<ObjectID, std::string> string_table_rev;
        std::unordered_map<std::shared_ptr<ast::Ident>, ObjectID> ident_table;
        std::unordered_map<ObjectID, std::shared_ptr<ast::Ident>> ident_table_rev;
        std::unordered_map<ObjectID, std::uint64_t> ident_index_table;
        std::unordered_map<std::shared_ptr<ast::StructType>, ObjectID> struct_table;
        std::vector<RangePacked> programs;
        std::vector<IdentRange> ident_to_ranges;
        std::vector<Code> code;
        std::uint64_t object_id = 1;
        std::vector<std::shared_ptr<CFG>> cfgs;

        std::unordered_map<std::uint64_t, ObjectID> immediate_table;
        std::optional<Varint> true_id;
        std::optional<Varint> false_id;

        std::unordered_map<std::string, Storages> storage_table;
        std::unordered_map<std::string, ObjectID> storage_key_table;
        std::unordered_map<ObjectID, std::string> storage_key_table_rev;

        expected<StorageRef> get_storage_ref(const Storages& s, brgen::lexer::Loc* loc) {
            if (s.length.value() != s.storages.size()) {
                return unexpect_error("Invalid storage length");
            }
            auto key = storage_key(s);
            if (auto found = storage_key_table.find(key); found != storage_key_table.end()) {
                auto k = varint(found->second);
                if (!k) {
                    return unexpect_error(std::move(k.error()));
                }
                return StorageRef{.ref = *k};
            }
            auto id = new_id(loc);
            if (!id) {
                return unexpect_error(std::move(id.error()));
            }
            storage_table.emplace(key, s);
            storage_key_table.emplace(key, id->value());
            storage_key_table_rev.emplace(id->value(), key);
            return StorageRef{.ref = id.value()};
        }

        expected<Storages> get_storage(StorageRef ref) {
            auto id = ref.ref.value();
            if (auto found = storage_key_table_rev.find(id); found != storage_key_table_rev.end()) {
                auto key = found->second;
                if (auto s = storage_table.find(key); s != storage_table.end()) {
                    return s->second;
                }
            }
            return unexpect_error("Invalid storage reference");
        }

        // internal
        bool on_encode_fn = false;
        std::optional<Varint> on_function;
        Endian global_endian = Endian::big;  // default to big endian
        Endian local_endian = Endian::unspec;
        ObjectID current_dynamic_endian = null_id;

        std::optional<Varint> union_ref;

        Varint get_function() const {
            if (on_function) {
                return *on_function;
            }
            return *varint(null_id);
        }

        auto enter_function(Varint id) {
            on_function = id;
            local_endian = global_endian;
            current_dynamic_endian = null_id;
            return futils::helper::defer([&] { on_function = std::nullopt; });
        }

        expected<EndianExpr> get_endian(Endian base, bool sign) {
            EndianExpr e;
            e.sign(sign);
            e.endian(base);
            if (base != Endian::unspec) {
                return e;
            }
            e.endian(global_endian);
            if (on_function) {
                e.endian(local_endian);
            }
            if (e.endian() == Endian::dynamic) {
                auto ref = varint(current_dynamic_endian);
                if (!ref) {
                    return unexpect_error(std::move(ref.error()));
                }
                e.dynamic_ref = ref.value();
            }
            return e;
        }

        bool set_endian(Endian e, ObjectID id = null_id) {
            if (on_function) {
                local_endian = e;
                current_dynamic_endian = id;
                return true;
            }
            if (e == Endian::dynamic) {
                return false;
            }
            global_endian = e;
            return true;
        }

        std::unordered_map<std::shared_ptr<ast::Node>, Varint> bit_field_begin;
        std::unordered_map<std::shared_ptr<ast::Node>, PackedOpType> bit_field_variability;
        std::unordered_set<std::shared_ptr<ast::Node>> bit_field_end;

        std::unordered_map<ObjectID, ObjectID> previous_assignments;

        std::vector<PhiStack> phi_stack;

        void init_phi_stack(ObjectID condition) {
            phi_stack.push_back(PhiStack{
                .start_state = previous_assignments,
                .current_condition = condition,
                .current_dynamic_endian = current_dynamic_endian,
                .current_endian = local_endian,
            });
        }

        void next_phi_candidate(ObjectID condition) {
            assert(phi_stack.size());
            phi_stack.back().candidates.push_back({
                .condition = phi_stack.back().current_condition,
                .candidate = std::move(previous_assignments),
            });
            current_dynamic_endian = phi_stack.back().current_dynamic_endian;
            local_endian = phi_stack.back().current_endian;
            previous_assignments = phi_stack.back().start_state;
            phi_stack.back().current_condition = condition;
        }

        PhiStack end_phi_stack() {
            assert(phi_stack.size());
            phi_stack.back().candidates.push_back({
                .condition = phi_stack.back().current_condition,
                .candidate = std::move(previous_assignments),
            });
            auto stack = std::move(phi_stack.back());
            current_dynamic_endian = stack.current_dynamic_endian;
            local_endian = stack.current_endian;
            phi_stack.pop_back();
            return stack;
        }

        void define_assign(ObjectID init) {
            previous_assignments[init] = init;
        }

        ObjectID prev_assign(ObjectID init) {
            if (auto found = previous_assignments.find(init); found != previous_assignments.end()) {
                return found->second;
            }
            return init;
        }

        void assign(ObjectID init, ObjectID assign) {
            if (auto found = previous_assignments.find(init); found != previous_assignments.end()) {
                previous_assignments[init] = assign;
            }
        }

        void map_struct(std::shared_ptr<ast::StructType> s, ObjectID id) {
            struct_table[s] = id;
        }

        Varint lookup_struct(std::shared_ptr<ast::StructType> s) {
            auto it = struct_table.find(s);
            if (it == struct_table.end()) {
                return Varint();
            }
            return *varint(it->second);  // this was an error!!!!!!
        }

       private:
        std::uint64_t prev_expr_id = null_id;

       public:
        expected<Varint> get_prev_expr() {
            if (prev_expr_id == null_id) {
                return unexpect_error("No previous expression");
            }
            auto expr = varint(prev_expr_id);
            prev_expr_id = null_id;
            return expr;
        }

        void set_prev_expr(std::uint64_t id) {
            prev_expr_id = id;
        }

        expected<Varint> lookup_metadata(const std::string& str, brgen::lexer::Loc* loc) {
            auto str_ref = metadata_table.find(str);
            if (str_ref == metadata_table.end()) {
                auto ident = new_id(loc);
                if (!ident) {
                    return unexpect_error(std::move(ident.error()));
                }
                metadata_table.emplace(str, ident->value());
                metadata_table_rev.emplace(ident->value(), str);
                return ident.value();
            }
            return varint(str_ref->second);
        }

        expected<std::pair<Varint, bool>> lookup_string(const std::string& str, brgen::lexer::Loc* loc) {
            auto str_ref = string_table.find(str);
            if (str_ref == string_table.end()) {
                auto ident = new_id(loc);
                if (!ident) {
                    return unexpect_error(std::move(ident.error()));
                }
                string_table.emplace(str, ident->value());
                string_table_rev.emplace(ident->value(), str);
                return std::pair{ident.value(), true};
            }
            return varint(str_ref->second).transform([](auto&& v) { return std::pair{v, false}; });
        }

        expected<Varint> lookup_ident(std::shared_ptr<ast::Ident> ident) {
            if (!ident) {
                return new_id(nullptr);  // ephemeral id
            }
            std::shared_ptr<ast::Ident> base;
            if (!ident->base.lock()) {  // for unresolved ident
                base = ident;
            }
            else {
                std::tie(base, std::ignore) = *ast::tool::lookup_base(ident);
            }
            auto it = ident_table.find(base);
            if (it == ident_table.end()) {
                auto id = new_node_id(base);
                if (!id) {
                    return id;
                }
                ident_table[base] = id->value();
                ident_table_rev[id->value()] = base;
                return id;
            }
            return varint(it->second);
        }

        expected<Varint> new_node_id(const std::shared_ptr<ast::Node>& node) {
            return new_id(&node->loc);
        }

        expected<Varint> new_id(brgen::lexer::Loc* loc) {
            return varint(object_id++);
        }

        void op(AbstractOp op) {
            Code c;
            c.op = op;
            code.push_back(std::move(c));
        }

        void op(AbstractOp op, auto&& set) {
            Code c;
            c.op = op;
            set(c);
            auto ident = c.ident();
            code.push_back(std::move(c));
            if (ident) {
                ident_index_table[ident->value()] = code.size() - 1;
            }
        }
    };

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node);

    Error save(Module& m, futils::binary::writer& w);

    Error transform(Module& m, const std::shared_ptr<ast::Node>& node);

    void write_cfg(futils::binary::writer& w, Module& m);
}  // namespace rebgn
