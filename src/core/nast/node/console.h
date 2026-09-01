/*license*/
#pragma once
#include <wrap/cout.h>

#include <cstdio>
#include <format>
#include <string_view>

// 端末への書き出し。**`std::print` / `std::println` を直に使わないこと。**
//
// コンソールの符号化は UTF-8 とは限らず、Windows では既定でそうではない。
// `std::print` はバイト列をそのまま渡すので、日本語や .bgn の文字列リテラルが
// 崩れる。futils の `wrap::cout_wrap()` は端末なら変換して書き、ファイルや
// パイプなら素通しするので、そちらを通す。
//
// 書式は `std::print` と同じ (第 1 引数に stderr も渡せる)。名前を変えて
// あるのは、引数に std の型が来ると ADL で `std::println` も候補に挙がり、
// 同じ名前だと呼び出しが曖昧になるため。

namespace brgen::nast {

    inline void write_console(std::FILE* to, std::string_view text) {
        auto& out = to == stderr ? futils::wrap::cerr_wrap() : futils::wrap::cout_wrap();
        out << text;
    }

    template <class... A>
    void print_text(std::format_string<A...> fmt, A&&... a) {
        write_console(stdout, std::format(fmt, std::forward<A>(a)...));
    }

    template <class... A>
    void print_line(std::format_string<A...> fmt, A&&... a) {
        write_console(stdout, std::format(fmt, std::forward<A>(a)...) + "\n");
    }

    template <class... A>
    void print_text(std::FILE* to, std::format_string<A...> fmt, A&&... a) {
        write_console(to, std::format(fmt, std::forward<A>(a)...));
    }

    template <class... A>
    void print_line(std::FILE* to, std::format_string<A...> fmt, A&&... a) {
        write_console(to, std::format(fmt, std::forward<A>(a)...) + "\n");
    }

}  // namespace brgen::nast
