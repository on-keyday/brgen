/*license*/
#include <comb2/composite/comment.h>
#include <comb2/composite/string.h>
#include <comb2/composite/range.h>
#include <comb2/basic/group.h>
#include <comb2/lexctx.h>
#include <optional>
#include <view/concat.h>

namespace hex2bin {
    using namespace futils::comb2;
    using namespace ops;
    namespace cps = composite;

    constexpr auto comment = cps::shell_comment;
    constexpr auto hex = cps::hex;
    constexpr auto space = cps::space | cps::tab | cps::eol;

    constexpr auto hex_tag = "hex";

    constexpr auto text = *(comment | space | str(hex_tag, hex)) & eos;

    namespace test {
        constexpr auto check_hex() {
            auto seq = futils::make_ref_seq("1234567890abcdefABCDEF # comment");
            return text(seq, 0, 0) == Status::match;
        }

        static_assert(check_hex());
    }  // namespace test

    struct HexReader : LexContext<> {
        std::string buf;
        void end_string(Status res, auto&& tag, auto&& seq, Pos pos) {
            LexContext::end_string(res, tag, seq, pos);
            if (res == Status::match) {
                for (auto i = pos.begin; i < pos.end; i++) {
                    buf.push_back(seq.buf.buffer[i]);
                }
            }
        }
    };

    template <class T>
    std::optional<std::string> read_hex(T&& t) {
        auto seq = futils::make_ref_seq(t);
        auto ctx = HexReader{};
        if (auto res = text(seq, ctx, 0); res == Status::match) {
            if (ctx.buf.size() % 2 == 0) {
                std::string data;
                for (auto i = 0; i < ctx.buf.size(); i += 2) {
                    futils::byte c = ctx.buf[i];
                    futils::byte d = ctx.buf[i + 1];
                    auto n = futils::number::number_transform[c] << 4 | futils::number::number_transform[d];
                    data.push_back(n);
                }
                return data;
            }
        }
        return std::nullopt;
    }

    template <class T>
    std::optional<std::string> read_hex_stream(T&& new_data, std::string& remain_buffer) {
        auto c = futils::view::make_concat(remain_buffer, new_data);
        auto ctx = HexReader{};
        auto seq = futils::make_ref_seq(c);
        if (auto res = text(seq, ctx, 0); res == Status::match) {
            std::string data;
            for (auto i = 0; i < ctx.buf.size() / 2; i++) {
                futils::byte c = ctx.buf[i << 1];
                futils::byte d = ctx.buf[(i << 1) + 1];
                auto n = futils::number::number_transform[c] << 4 | futils::number::number_transform[d];
                data.push_back(n);
            }
            if (ctx.buf.size() % 2) {
                remain_buffer = std::string(ctx.buf.end() - 1, ctx.buf.end());
            }
            else {
                remain_buffer.clear();
            }
            return data;
        }
        return std::nullopt;
    }

}  // namespace hex2bin
