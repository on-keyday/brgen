/*license*/
#include "stub/structs.hpp"
#include <type_traits>
#include "../ebmgen/common.hpp"
#include "ebm/extended_binary_module.hpp"
namespace ebmcodegen {
    template <typename Root>
    void retrieve(std::map<std::string_view, Struct>& struct_map, std::map<std::string_view, Enum>& enum_map, bool include_refs) {
        std::vector<Struct> structs;
        structs.push_back({
            Root::visitor_name,
        });
        Root::visit_static([&](auto&& visitor, const char* name, auto tag, TypeAttribute dispatch = NONE) -> void {
            using T = typename decltype(tag)::type;
            constexpr bool is_rvalue = decltype(tag)::is_rvalue;
            if (is_rvalue) {
                dispatch = TypeAttribute(static_cast<int>(dispatch) | static_cast<int>(TypeAttribute::RVALUE));
            }
            if constexpr (ebmgen::has_visit<T, decltype(visitor)>) {
                structs.back().fields.push_back({
                    name,
                    T::visitor_name,
                    dispatch,
                });
                constexpr auto is_ref = ebmgen::AnyRef<T> || std::is_same_v<T, ebm::Varint>;
                if (!is_ref || include_refs) {
                    structs.push_back({
                        .name = T::visitor_name,
                        .is_any_ref = is_ref,
                    });
                    T::visit_static(visitor);
                    auto s = std::move(structs.back());
                    structs.pop_back();
                    struct_map[s.name] = std::move(s);
                }
            }
            else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
                using P = typename futils::helper::template_instance_of_t<T, std::vector>::template param_at<0>;
                visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::ARRAY));
            }
            else if constexpr (std::is_pointer_v<T>) {
                using P = std::remove_pointer_t<T>;
                visitor(visitor, name, ebm::ExtendedBinaryModule::visitor_tag<P>{}, TypeAttribute(dispatch | TypeAttribute::PTR));
            }
            else if constexpr (std::is_enum_v<T>) {
                constexpr const char* enum_name = visit_enum(T{});
                structs.back().fields.push_back({
                    name,
                    enum_name,
                    dispatch,
                });
                if (!enum_map.contains(enum_name)) {
                    auto& added = enum_map[enum_name];
                    added.name = enum_name;

                    auto do_collect = [&](auto&& cond) {
                        for (size_t i = 0; cond(i); i++) {
                            auto name = to_string(T(i), true);
                            if (name[0] == 0) {
                                continue;
                            }
                            auto alt_name = to_string(T(i), false);
                            if (std::string_view(alt_name) != name) {
                                added.members.push_back({
                                    .name = name,
                                    .value = i,
                                    .alt_name = alt_name,
                                });
                                continue;
                            }
                            added.members.push_back({
                                .name = to_string(T(i), true),
                                .value = i,
                            });
                        }
                    };
                    if constexpr (std::is_same_v<T, ebm::OpCode>) {  // edge case
                        do_collect([&](size_t i) {
                            return i <= static_cast<size_t>(ebm::OpCode::MAX_OPCODE);
                        });
                    }
                    else {
                        do_collect([&](size_t i) {
                            return to_string(T(i))[0] != 0;
                        });
                    }
                }
            }
            else if constexpr (std::is_same_v<T, std::uint64_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::uint64_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::uint8_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::uint8_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::int32_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::int32_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::int8_t>) {
                structs.back().fields.push_back({
                    name,
                    "std::int8_t",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, float>) {
                structs.back().fields.push_back({
                    name,
                    "float",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, double>) {
                structs.back().fields.push_back({
                    name,
                    "double",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, bool>) {
                structs.back().fields.push_back({
                    name,
                    "bool",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                structs.back().fields.push_back({
                    name,
                    "std::string",
                    dispatch,
                });
            }
            else if constexpr (std::is_same_v<T, const char(&)[5]>) {  // skip
            }
            else {
                static_assert(std::is_same_v<T, void>, "Unsupported type");
            }
        });
        struct_map[Root::visitor_name] = std::move(structs[0]);
    }

    std::pair<std::map<std::string_view, Struct>, std::map<std::string_view, Enum>> make_struct_map(bool include_refs) {
        std::map<std::string_view, Struct> struct_map;
        std::map<std::string_view, Enum> enum_map;

        retrieve<ebm::ExtendedBinaryModule>(struct_map, enum_map, include_refs);
        retrieve<ebm::Instruction>(struct_map, enum_map, include_refs);

        return {struct_map, enum_map};
    }
}  // namespace ebmcodegen
