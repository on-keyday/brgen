/*license*/
#include "stream.h"
#include <mutex>

namespace ast {
    std::unique_ptr<Program> parse(Stream& s) {
        auto prog = std::make_unique<Program>();
        while (!s.eos()) {
            s.skip_tag(lexer::Tag::space, lexer::Tag::line);
            auto token = s.must_consume_token(lexer::Tag::int_literal);
            auto ilit = std::make_unique<IntLiteral>();
            ilit->loc = token.loc;
            ilit->raw = token.token;
            prog->program.push_back(std::move(ilit));
        }
        return prog;
    }

}  // namespace ast
