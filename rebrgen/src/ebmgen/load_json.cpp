/*license*/
#include <file/file_view.h>
#include <core/ast/file.h>
#include <core/ast/json.h>
#include <json/parser.h>
#include <json/constructor.h>
#include <string_view>
#include "load_json.hpp"
#include "common.hpp"
#include "ebm/extended_binary_module.hpp"
#include "json_conv.hpp"
#include <binary/discard.h>

namespace ebmgen {
    struct DecoderObserver {
        std::uint64_t node_count = 0;
        std::uint64_t scope_count = 0;
        bool next_int_is_node_count = false;
        bool next_int_is_scope_count = false;
        bool next_array_is_node = false;
        bool next_array_is_scope = false;
        void observe(futils::json::ElementType t, auto& s) {
            if (t == futils::json::ElementType::key_string) {
                if (s.size() != 4) {
                    return;
                }
                if (s.back().get_holder().init_as_string() == "node_count") {
                    next_int_is_node_count = true;
                }
                else if (s.back().get_holder().init_as_string() == "scope_count") {
                    next_int_is_scope_count = true;
                }
                else if (s.back().get_holder().init_as_string() == "node") {
                    next_array_is_node = true;
                }
                else if (s.back().get_holder().init_as_string() == "scope") {
                    next_array_is_scope = true;
                }
            }
            else if (t == futils::json::ElementType::integer) {
                if (next_int_is_node_count) {
                    node_count = *s.back().get_holder().as_numi();
                    next_int_is_node_count = false;
                }
                else if (next_int_is_scope_count) {
                    scope_count = *s.back().get_holder().as_numi();
                    next_int_is_scope_count = false;
                }
            }
            else if (t == futils::json::ElementType::array) {
                if (next_array_is_node) {
                    next_array_is_node = false;
                    s.back().get_holder().init_as_array().reserve(node_count);
                }
                else if (next_array_is_scope) {
                    next_array_is_scope = false;
                    s.back().get_holder().init_as_array().reserve(scope_count);
                }
            }
        }
    };

    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json_file(futils::view::rvec input, std::function<void(const char*)> timer_cb) {
        if (timer_cb) timer_cb("json file open");
        futils::json::BytesLikeReader<futils::view::rvec> r{input};
        r.size = r.bytes.size();
        futils::json::JSONConstructor<futils::json::JSON, futils::json::StaticStack<15, futils::json::JSON>, DecoderObserver> jc;
        futils::json::GenericConstructor<decltype(r)&, futils::json::StaticStack<8, futils::json::ParseStateDetail>, decltype(jc)&> g{r, jc};
        futils::json::Parser<decltype(g)&> p{g};
        p.skip_space();
        auto result = p.parse();
        if (result != futils::json::ParseResult::end) {
            return unexpect_error("cannot parse json file: {} {}", futils::json::to_string(result), futils::json::to_string(p.state.state()));
        }
        if (timer_cb) timer_cb("json file parse");
        auto js = std::move(jc.stack.back());
        brgen::ast::AstFile file;
        if (!futils::json::convert_from_json(js, file)) {
            return unexpect_error("cannot convert json file");
        }
        if (!file.ast) {
            return unexpect_error("ast is not found");
        }
        if (timer_cb) timer_cb("json file convert");
        brgen::ast::JSONConverter c;
        auto res = c.decode(*file.ast);
        if (!res) {
            return unexpect_error("cannot decode json file: {}", res.error().locations[0].msg);
        }
        if (!*res) {
            return unexpect_error("cannot decode json file");
        }
        if (timer_cb) timer_cb("json file decode");
        return std::pair{*res, file.files};
    }

    expected<std::pair<std::shared_ptr<brgen::ast::Node>, std::vector<std::string>>> load_json(std::string_view input, std::function<void(const char*)> timer_cb) {
        futils::file::View view;
        if (auto res = view.open(input); !res) {
            return unexpect_error(Error(res.error()));
        }
        return load_json_file(futils::view::rvec(view), timer_cb);
    }

    struct JSONEBMObserver {
        std::uint64_t len = 0;
        bool next_int_is_array_len = false;
        void observe(futils::json::ElementType t, auto& s) {
            if (t == futils::json::ElementType::key_string) {
                if (s.back().get_holder().init_as_string().ends_with("len")) {
                    next_int_is_array_len = true;
                }
            }
            else if (t == futils::json::ElementType::integer) {
                if (next_int_is_array_len) {
                    len = *s.back().get_holder().as_numi();
                    next_int_is_array_len = false;
                }
            }
            else if (t == futils::json::ElementType::array) {
                if (len != 0) {
                    s.back().get_holder().init_as_array().reserve(len);
                    len = 0;
                }
            }
        }
    };

    expected<ebm::ExtendedBinaryModule> decode_json_ebm(futils::view::rvec input) {
        futils::json::BytesLikeReader<futils::view::rvec> r{input};
        r.size = r.bytes.size();
        futils::json::JSONConstructor<futils::json::JSON, futils::json::StaticStack<15, futils::json::JSON>, JSONEBMObserver> jc;
        futils::json::GenericConstructor<decltype(r)&, futils::json::StaticStack<8, futils::json::ParseStateDetail>, decltype(jc)&> g{r, jc};
        futils::json::Parser<decltype(g)&> p{g};
        p.skip_space();
        auto result = p.parse();
        if (result != futils::json::ParseResult::end) {
            return unexpect_error("cannot parse json file: {} {}", futils::json::to_string(result), futils::json::to_string(p.state.state()));
        }
        auto js = std::move(jc.stack.back());
        ebm::ExtendedBinaryModule ebm;
        ///*
        if (!futils::json::convert_from_json(js, ebm)) {
            return unexpect_error("cannot convert json file");
        }
        //*/
        // return unexpect_error("not implemented yet");
        // check this is encodable
        futils::binary::writer w{&futils::binary::discard<>, nullptr};
        auto err = ebm.encode(w);
        if (err) {
            return unexpect_error("cannot encode ebm: {}", err.error<std::string>());
        }
        return ebm;
    }

    expected<ebm::ExtendedBinaryModule> load_json_ebm(std::string_view input) {
        futils::file::View view;
        if (auto res = view.open(input); !res) {
            return unexpect_error(Error(res.error()));
        }
        return decode_json_ebm(futils::view::rvec(view));
    }

    expected<void> load_ebm_to_repository(EBMRepository& repo, ebm::ExtendedBinaryModule&& ebm) {
        TestRepositoryAccessor accessor(repo);

        // Set max_id
        accessor.set_max_id(get_id(ebm.max_id));

        // Load identifiers
        for (auto& item : ebm.identifiers) {
            auto result = accessor.add_identifier_with_id(item.id, std::move(item.body));
            if (!result) {
                return unexpect_error(std::move(result.error()));
            }
        }

        // Load strings
        for (auto& item : ebm.strings) {
            auto result = accessor.add_string_with_id(item.id, std::move(item.body));
            if (!result) {
                return unexpect_error(std::move(result.error()));
            }
        }

        // Load types
        for (auto& item : ebm.types) {
            auto result = accessor.add_type_with_id(item.id, std::move(item.body));
            if (!result) {
                return unexpect_error(std::move(result.error()));
            }
        }

        // Load statements
        for (auto& item : ebm.statements) {
            auto result = accessor.add_statement_with_id(item.id, std::move(item.body));
            if (!result) {
                return unexpect_error(std::move(result.error()));
            }
        }

        // Load expressions
        for (auto& item : ebm.expressions) {
            auto result = accessor.add_expression_with_id(item.id, std::move(item.body));
            if (!result) {
                return unexpect_error(std::move(result.error()));
            }
        }

        // Load aliases
        accessor.load_aliases(std::move(ebm.aliases));

        // Load debug info
        accessor.load_debug_info(std::move(ebm.debug_info.files), std::move(ebm.debug_info.locs));

        // Recalculate internal caches
        accessor.recalculate_caches();

        return {};
    }

    expected<void> TestContextLoader::load_json_ebm_file(std::string_view file_path) {
        MAYBE(ebm, load_json_ebm(file_path));
        return load_ebm(std::move(ebm));
    }

    expected<void> TestContextLoader::load_json_ebm_string(std::string_view json_str) {
        MAYBE(ebm, decode_json_ebm(futils::view::rvec(json_str.data(), json_str.size())));
        return load_ebm(std::move(ebm));
    }

    expected<void> TestContextLoader::load_ebm(ebm::ExtendedBinaryModule&& ebm) {
        return load_ebm_to_repository(ctx_.repository(), std::move(ebm));
    }

    expected<ebm::ExtendedBinaryModule> TestContextLoader::finalize(bool verify_uniqueness) {
        ebm::ExtendedBinaryModule result;
        MAYBE_VOID(f, ctx_.repository().finalize(result, verify_uniqueness));
        return result;
    }
}  // namespace ebmgen
