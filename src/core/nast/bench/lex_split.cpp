// 同じ字句解析を 3 通りで回して、Stream の上乗せがどこかを見る。
//   A: parse_one を直接 (std::string 上の Sequencer)
//   B: File::parse を直接ループ (実際の buffer 型 + 関数ポインタ経由)
//   C: Stream 経由 (トークンを list に積み、行桁を数え、shrink する)
#include "../node/console.h"
#include "../parse/stream.h"
#include <core/common/file.h>
#include <core/lexer/lexer.h>
#include <chrono>
#include <string>
#include <vector>

using brgen::nast::print_line;
using brgen::nast::print_text;

using clock_ = std::chrono::steady_clock;
static double ms(std::chrono::nanoseconds d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

int main(int argc, char** argv) {
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) paths.push_back(argv[i]);
    std::chrono::nanoseconds ta{}, tb{}, tc{};
    std::size_t na = 0, nb = 0, nc = 0, bytes = 0;

    for (auto& p : paths) {
        brgen::FileSet fs;
        auto l = fs.add_file(p);
        if (!l) continue;
        auto* f = fs.get_input(*l);
        if (!f) continue;
        auto src = f->source();
        std::string text(reinterpret_cast<const char*>(src.data()), src.size());
        bytes += text.size();
        auto seq = futils::make_ref_seq(text);
        auto t = clock_::now();
        while (auto tok = brgen::lexer::parse_one(seq, 0, brgen::lexer::Option{})) {
            if (tok->tag == brgen::lexer::Tag::error) break;
            na++;
        }
        ta += clock_::now() - t;
    }

    for (auto& p : paths) {
        brgen::FileSet fs;
        auto l = fs.add_file(p);
        if (!l) continue;
        auto* f = fs.get_input(*l);
        if (!f) continue;
        auto t = clock_::now();
        while (auto tok = f->parse(brgen::lexer::Option{})) {
            if (tok->tag == brgen::lexer::Tag::error) break;
            nb++;
        }
        tb += clock_::now() - t;
    }

    for (auto& p : paths) {
        brgen::FileSet fs;
        auto l = fs.add_file(p);
        if (!l) continue;
        auto* f = fs.get_input(*l);
        if (!f) continue;
        brgen::nast::Context ctx;
        auto t = clock_::now();
        (void)ctx.enter_stream(f, [&](brgen::nast::Stream& s) -> brgen::nast::Node<brgen::nast::Module> {
            while (!s.eos()) {
                s.consume();
                nc++;
            }
            return brgen::nast::nullref;
        });
        tc += clock_::now() - t;
    }

    // B2: 同じ File 経由だが、バッファを std::string で持たせた場合。
    // add_file が使うバッファ型のせいなのか、関数ポインタ経由のせいなのかを分ける。
    std::chrono::nanoseconds tb2{};
    std::size_t nb2 = 0;
    for (auto& p : paths) {
        brgen::FileSet fs;
        auto l = fs.add_file(p);
        if (!l) continue;
        auto* f0 = fs.get_input(*l);
        if (!f0) continue;
        auto src = f0->source();
        std::string text(reinterpret_cast<const char*>(src.data()), src.size());
        brgen::File f;
        brgen::make_file_from_text<std::string>(f, std::move(text));
        auto t = clock_::now();
        while (auto tok = f.parse(brgen::lexer::Option{})) {
            if (tok->tag == brgen::lexer::Tag::error) break;
            nb2++;
        }
        tb2 += clock_::now() - t;
    }

    print_line("{} files, {} bytes, tokens A={} B={} C={}", paths.size(), bytes, na, nb, nc);
    print_line("A parse_one direct   {:8.1f} ms", ms(ta));
    print_line("B File::parse loop   {:8.1f} ms  (+{:.1f} over A)", ms(tb), ms(tb - ta));
    print_line("C through Stream     {:8.1f} ms  (+{:.1f} over B)", ms(tc), ms(tc - tb));
    print_line("B2 File over string  {:8.1f} ms  (tokens {})", ms(tb2), nb2);
}
