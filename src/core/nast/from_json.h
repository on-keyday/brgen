/*license*/
#pragma once
#include "nodes.h"

#include <json/json_export.h>
#include <concepts>
#include <string>
#include <type_traits>
#include <vector>

// Arena::as_json の逆。ebmgen/json_conv.cpp と同じく futils::json::JSON を読むが、
// フィールドの列挙は nodes.h の for_each_field / for_each_pool に任せるので、
// 型ごとの from_json を生成する必要がない。
namespace brgen::nast {

    using JSON = futils::json::JSON;

    // 添字で埋められる列。std::vector と StablePool の両方が該当する。
    // std::string も同じ操作を持つので明示的に除く。
    template <class V>
    concept json_array = !std::is_same_v<V, std::string> && requires(V v) {
        typename V::value_type;
        v.clear();
        v.resize(std::size_t{});
        { v.size() } -> std::convertible_to<std::size_t>;
        v[std::size_t{}];
    };

    // for_each_field 版が列を含みうるので、こちらだけ先に宣言する
    template <json_array V>
    bool read_json(const JSON& j, V& v);

    inline bool read_json(const JSON& j, std::string& v) {
        return j.as_string(v);
    }

    inline bool read_json(const JSON& j, bool& v) {
        return j.as_bool(v);
    }

    inline bool read_json(const JSON& j, lexer::Loc& v) {
        // as_json 側は Loc をオブジェクトとして出す
        auto get = [&](const char* name, auto& dst) {
            auto f = j.at(name);
            return f && f->as_number(dst);
        };
        std::uint64_t begin = 0, end = 0, file = 0, line = 0, col = 0;
        if (auto pos = j.at("pos")) {
            auto b = pos->at("begin");
            auto e = pos->at("end");
            if (b) {
                b->as_number(begin);
            }
            if (e) {
                e->as_number(end);
            }
        }
        get("file", file);
        get("line", line);
        get("col", col);
        v.pos.begin = static_cast<size_t>(begin);
        v.pos.end = static_cast<size_t>(end);
        v.file = static_cast<lexer::FileIndex>(file);
        v.line = static_cast<size_t>(line);
        v.col = static_cast<size_t>(col);
        return true;
    }

    template <class T>
    bool read_json(const JSON& j, Node<T>& v) {
        // Node は unique_id (type<<32 | id) の数値として出ている
        std::uint64_t raw = 0;
        if (!j.as_number(raw)) {
            return false;
        }
        v = Node<T>::from_unique_id(raw);
        return true;
    }

    template <class V>
        requires(std::is_enum_v<V>)
    bool read_json(const JSON& j, V& v) {
        std::string s;
        if (!read_json(j, s)) {
            return false;
        }
        auto got = from_string<V>(s);
        if (!got) {
            return false;
        }
        v = *got;
        return true;
    }

    template <class V>
        requires(std::is_integral_v<V> && !std::is_same_v<V, bool>)
    bool read_json(const JSON& j, V& v) {
        std::uint64_t raw = 0;
        if (!j.as_number(raw)) {
            return false;
        }
        v = static_cast<V>(raw);
        return true;
    }

    // for_each_field を持つもの (NodeData<T> / NodeHeader / 値型) はこれで読める
    template <class V>
        requires requires(V v) { v.for_each_field([](const char*, auto&, bool) {}); }
    bool read_json(const JSON& j, V& v) {
        bool ok = true;
        v.for_each_field([&](const char* name, auto& field, bool) {
            if (!ok) {
                return;
            }
            if (auto got = j.at(name)) {
                if (!read_json(*got, field)) {
                    ok = false;
                }
            }
        });
        return ok;
    }

    template <json_array V>
    bool read_json(const JSON& j, V& v) {
        if (!j.is_array()) {
            return false;
        }
        v.clear();
        v.resize(j.size());
        for (std::size_t i = 0; i < j.size(); i++) {
            auto e = j.at(i);
            if (!e || !read_json(*e, v[i])) {
                return false;
            }
        }
        return true;
    }

    inline bool from_json(Arena& a, const JSON& j) {
        if (!j.is_object()) {
            return false;
        }
        auto headers = j.at("headers");
        if (!headers || !read_json(*headers, a.raw_headers())) {
            return false;
        }
        bool ok = true;
        a.for_each_pool([&](const char* name, auto& pool) {
            if (!ok) {
                return;
            }
            auto got = j.at(name);
            if (!got) {
                pool.clear();
                return;
            }
            if (!read_json(*got, pool)) {
                ok = false;
            }
        });
        return ok;
    }

}  // namespace brgen::nast
