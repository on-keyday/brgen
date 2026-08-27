// トークン本文を作る代金。
//
// File::parse は本文つきの Token (88 バイト) を、File::parse_no_text は
// tag と Loc だけの LiteToken (48 バイト) を返す。両方が残っているので、
// 本文の切り出しと、それを容器へ積む代金を並べて測れる。
#include <core/common/file.h>
#include <core/lexer/lexer.h>

#include <chrono>
#include <list>
#include <print>
#include <string>
#include <vector>

using clock_ = std::chrono::steady_clock;

static double ms(std::chrono::nanoseconds d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

int main(int argc, char** argv) {
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) {
        paths.push_back(argv[i]);
    }
    std::size_t sink = 0, n_full = 0, n_lite = 0, bytes_full = 0, bytes_lite = 0;

    // 3 回の最速を採る。このマシンは周波数が安定しないので、
    // 条件どうしを比べるときは呼び出し側で交互に回すこと (README を見よ)。
    auto run = [&](const char* name, auto&& body) {
        std::chrono::nanoseconds best{};
        for (int r = 0; r < 3; r++) {
            std::chrono::nanoseconds total{};
            for (auto& p : paths) {
                brgen::FileSet fs;
                auto l = fs.add_file(p);
                if (!l) {
                    continue;
                }
                auto* f = fs.get_input(*l);
                if (!f) {
                    continue;
                }
                auto t = clock_::now();
                body(f);
                total += clock_::now() - t;
            }
            if (r == 0 || total < best) {
                best = total;
            }
        }
        std::println("{:<40} {:8.1f} ms", name, ms(best));
        return best;
    };

    std::println("sizeof(Token) = {}, sizeof(LiteToken) = {}", sizeof(brgen::lexer::Token),
                 sizeof(brgen::lexer::LiteToken));

    auto a = run("1 text, discarded", [&](brgen::File* f) {
        while (auto t = f->parse(brgen::lexer::Option{})) {
            if (t->tag == brgen::lexer::Tag::error) {
                break;
            }
            n_full++;
            bytes_full += t->token.size();
            sink += t->token.size();
        }
    });
    auto b = run("2 no text, discarded", [&](brgen::File* f) {
        while (auto t = f->parse_no_text(brgen::lexer::Option{})) {
            if (t->tag == brgen::lexer::Tag::error) {
                break;
            }
            n_lite++;
            bytes_lite += t->loc.pos.len();
            sink += t->loc.pos.len();
        }
    });
    auto c = run("3 text, into a list", [&](brgen::File* f) {
        std::list<brgen::lexer::Token> v;
        while (auto t = f->parse(brgen::lexer::Option{})) {
            if (t->tag == brgen::lexer::Tag::error) {
                break;
            }
            v.push_back(std::move(*t));
        }
        sink += v.size();
    });
    auto d = run("4 no text, into a list", [&](brgen::File* f) {
        std::list<brgen::lexer::LiteToken> v;
        while (auto t = f->parse_no_text(brgen::lexer::Option{})) {
            if (t->tag == brgen::lexer::Tag::error) {
                break;
            }
            v.push_back(*t);
        }
        sink += v.size();
    });

    std::println("");
    std::println("building the text        {:.1f} ms", ms(a - b));
    std::println("list push, with text    +{:.1f} ms", ms(c - a));
    std::println("list push, without      +{:.1f} ms", ms(d - b));
    std::println("");
    std::println("tokens  full={} lite={}   bytes full={} lite={}   (should match)",
                 n_full / 3, n_lite / 3, bytes_full / 3, bytes_lite / 3);
    std::println("(checksum {})", sink);
}
