/*license*/
#pragma once
#include <filesystem>
#include "../lexer/token.h"
#include "../lexer/lexer.h"
#include <code/src_location.h>
#include <helper/disable_self.h>
#include "error.h"
#include <map>
#include <variant>
#include <file/file_view.h>

namespace brgen {
    namespace fs = std::filesystem;

    struct Input {
       private:
        template <class T>
        static std::optional<lexer::Token> do_parse(void* ptr, std::uint64_t file) {
            return lexer::parse_one(*static_cast<utils::Sequencer<T>*>(ptr), file);
        }

        template <class T>
        static std::pair<std::string, utils::code::SrcLoc> dump_source(void* ptr, lexer::Pos pos) {
            auto& seq = *static_cast<utils::Sequencer<T>*>(ptr);
            size_t rptr = seq.rptr;
            seq.rptr = pos.begin;
            std::string out;
            auto loc = utils::code::write_src_loc(out, seq, pos.len());
            seq.rptr = rptr;
            return {out, loc};
        }
        lexer::FileIndex file = lexer::builtin;
        std::shared_ptr<void> ptr;
        std::optional<lexer::Token> (*parse_)(void* seq, std::uint64_t file) = nullptr;
        std::pair<std::string, utils::code::SrcLoc> (*dump_)(void* seq, lexer::Pos pos) = nullptr;

       public:
        Input() = default;
        template <class T, helper_disable_self(Input, T)>
        Input(T&& t, lexer::FileIndex fd) {
            using V = std::decay_t<T>;
            ptr = std::make_unique<utils::Sequencer<V>>(std::forward<T>(t));
            parse_ = do_parse<V>;
            dump_ = dump_source<V>;
            file = fd;
        }

        Input(const Input& i)
            : ptr(i.ptr), parse_(i.parse_), dump_(i.dump_), file(i.file) {}

        Input(Input&& i)
            : ptr(std::move(i.ptr)), parse_(std::exchange(i.parse_, nullptr)), dump_(std::exchange(i.dump_, nullptr)), file(std::exchange(i.file, lexer::builtin)) {}
        Input& operator=(const Input& i) {
            ptr = i.ptr;
            parse_ = i.parse_;
            dump_ = i.dump_;
            file = i.file;
            return *this;
        }

        Input& operator=(Input&& i) {
            ptr = std::move(i.ptr);
            parse_ = std::exchange(i.parse_, nullptr);
            dump_ = std::exchange(i.dump_, nullptr);
            file = std::exchange(i.file, lexer::builtin);
            return *this;
        }

        std::optional<lexer::Token> parse() {
            if (parse_) {
                return parse_(ptr.get(), file);
            }
            return std::nullopt;
        }

        std::pair<std::string, utils::code::SrcLoc> dump(lexer::Pos pos) {
            if (dump_) {
                return dump_(ptr.get(), pos);
            }
            return {};
        }

        StreamError error(std::string&& msg, lexer::Pos pos) {
            auto [src, loc] = dump(pos);
            return StreamError{
                std::move(msg),
                file,
                loc,
                std::move(src),
            };
        }
    };

    struct File {
        fs::path path;
        lexer::FileIndex index = lexer::builtin;
        std::optional<Input> input;
    };

    struct FileList {
       private:
        std::map<fs::path, File> files;
        std::map<lexer::FileIndex, File*> indexs;
        lexer::FileIndex index = lexer::builtin;

       public:
        std::variant<lexer::FileIndex, std::error_code> add(const auto& name) {
            fs::path path = utils::utf::convert<std::u8string>(name);
            std::error_code err;
            path = fs::absolute(path, err);
            if (err) {
                return err;
            }
            if (auto found = files.find(path); found != files.end()) {
                return found->second.index;
            }
            File file;
            file.path = std::move(path);
            file.index = index + 1;
            index++;
            auto& f = files[file.path];
            f = std::move(file);
            indexs[index] = &f;
            return files.size();
        }

        std::optional<Input> get_input(lexer::FileIndex fd) {
            if (fd < 1 || index < fd) {
                return std::nullopt;
            }
            auto& file = indexs[fd];
            if (!file->input) {
                utils::file::View v;
                if (!v.open(file->path.c_str())) {
                    return std::nullopt;
                }
                file->input = Input(std::move(v), file->index);
            }
            return file->input;
        }

        std::optional<fs::path> get_path(lexer::FileIndex fd) {
            if (fd < 1 || index < fd) {
                return std::nullopt;
            }
            auto& file = indexs[fd];
            return file->path;
        }
    };
}  // namespace brgen
