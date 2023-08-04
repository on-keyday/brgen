/*license*/
#pragma once
#include "../lexer/token.h"
#include "../lexer/lexer.h"
#include <list>
#include <optional>
#include <helper/defer.h>
#include <code/src_location.h>

namespace ast {

    struct StreamError {
        lexer::Token err_token;
        std::string src;
    };

    struct ContextInfo {};

    struct Stream {
       private:
        std::list<lexer::Token> tokens;
        using iterator = typename std::list<lexer::Token>::iterator;
        iterator cur;
        void* seq_ptr = nullptr;
        std::uint64_t cur_file = 0;
        ContextInfo* info = nullptr;
        std::optional<lexer::Token> (*parse)(void* seq, std::uint64_t file) = nullptr;
        std::string (*dump)(void* seq, lexer::Pos pos) = nullptr;

        template <class T>
        static std::optional<lexer::Token> do_parse(void* ptr, std::uint64_t file) {
            return lexer::parse_one(*static_cast<utils::Sequencer<T>*>(ptr), file);
        }

        template <class T>
        static std::string dump_source(void* ptr, lexer::Pos pos) {
            auto& seq = *static_cast<utils::Sequencer<T>*>(ptr);
            seq.rptr = pos.begin;
            std::string out;
            utils::code::write_src_loc(out, seq, pos.len());
            return out;
        }

        std::optional<lexer::Token> call_parse() {
            if (parse) {
                return parse(seq_ptr, cur_file);
            }
            return std::nullopt;
        }

        void report_error(lexer::Token& token) {
            auto text = dump(seq_ptr, token.loc.pos);
            throw StreamError{.err_token = std::move(token), std::move(text)};
        }

        Stream() = default;
        friend struct Context;

        void maybe_parse() {
            if (cur == tokens.end()) {
                auto token = call_parse();
                if (!token) {
                    return;
                }
                if (token->tag == lexer::Tag::error) {
                    report_error(*token);
                }
                cur = tokens.insert(cur, std::move(*token));
            }
        }

       public:
        template <class T>
        [[nodsicard]] auto set_seq(utils::Sequencer<T>& seq, std::uint64_t file) {
            auto old_parse = parse;
            auto old_dump = dump;
            auto old_ptr = seq_ptr;
            auto old_file = cur_file;

            seq_ptr = std::addressof(seq);
            parse = do_parse<T>;
            dump = dump_source<T>;
            cur_file = file;
            return utils::helper::defer([=, this] {
                cur_file = old_file;
                seq_ptr = old_ptr;
                parse = old_parse;
                dump = old_dump;
            });
        }

        void add_token(auto&& input, lexer::FileIndex file = lexer::builtin) {
            auto seq = utils::make_ref_seq(input);
            auto s = set_seq(seq, file);
            std::list<lexer::Token> tmp;
            for (auto input = call_parse(); input; input = call_parse()) {
                if (input->tag == lexer::Tag::error) {
                    report_error(*input);
                }
                tmp.push_back(std::move(*input));
            }
            tokens.splice(cur, std::move(tmp));
        }

        // end of stream
        bool eos() {
            maybe_parse();
            return cur == tokens.end();
        }

        void consume() {
            if (eos()) {
                return;
            }
            cur++;
        }

        bool expect_tag(lexer::Tag tag) {
            if (eos()) {
                return false;
            }
            return cur->tag == tag;
        }

        bool expect_token(std::string_view s) {
            if (eos()) {
                return false;
            }
            return cur->token == s;
        }

        std::optional<lexer::Token> consume_token(std::string_view s) {
            if (!expect_token(s)) {
                return std::nullopt;
            }
            // only copy for fallback
            auto tok = *cur;
            consume();
            return tok;
        }

        std::optional<lexer::Token> consume_token(lexer::Tag t) {
            if (!expect_tag(t)) {
                return std::nullopt;
            }
            // only copy for fallback
            auto tok = *cur;
            consume();
            return tok;
        }

        void skip_tag(auto... t) {
            while (!eos()) {
                if ((... || expect_tag(t))) {
                    consume();
                    continue;
                }
                break;
            }
        }

        ContextInfo* context() const {
            return info;
        }

       private:
        std::optional<StreamError> enter_stream(auto&& fn) {
            try {
                cur = tokens.begin();
                fn();
            } catch (StreamError& err) {
                return err;
            }
            return std::nullopt;
        }
    };

    struct Context {
       private:
        Stream stream;
        ContextInfo info;

       public:
        template <class T>
        std::optional<StreamError> enter_stream(utils::Sequencer<T>& seq, lexer::FileIndex file, auto&& fn) {
            stream.info = &info;
            return stream.enter_stream([&] {
                const auto scope = stream.set_seq(seq, file);
                fn(stream);
            });
        }
    };
}  // namespace ast
