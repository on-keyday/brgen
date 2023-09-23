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
#include "expected.h"

namespace brgen {
    namespace fs = std::filesystem;

    struct File {
       private:
        friend struct FileSet;
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
        fs::path file_name;
        std::shared_ptr<void> ptr;
        std::optional<lexer::Token> (*parse_)(void* seq, std::uint64_t file) = nullptr;
        std::pair<std::string, utils::code::SrcLoc> (*dump_)(void* seq, lexer::Pos pos) = nullptr;

        template <class T>
        void set_input(T&& t) {
            using U = std::decay_t<T>;
            ptr = std::make_unique<utils::Sequencer<U>>(std::forward<T>(t));
            parse_ = do_parse<U>;
            dump_ = dump_source<U>;
        }

       public:
        File() = default;

        File(lexer::FileIndex fd, std::string&& name) {
            file = fd;
            file_name = std::move(name);
        }

        File(const File&) = delete;

        std::optional<lexer::Token> parse() {
            if (parse_) {
                return parse_(ptr.get(), file);
            }
            return std::nullopt;
        }

       private:
        std::pair<std::string, utils::code::SrcLoc> dump(lexer::Pos pos) {
            if (dump_) {
                return dump_(ptr.get(), pos);
            }
            return {};
        }

       public:
        SourceEntry error(std::string&& msg, lexer::Pos pos, bool warn = false) {
            auto [src, loc] = dump(pos);
            return SourceEntry{
                std::move(msg),
                file_name.generic_string(),
                loc,
                std::move(src),
                warn,
            };
        }

        const fs::path& path() const {
            return file_name;
        }
    };

    struct FileSet {
       private:
        std::map<fs::path, File> files;
        std::map<lexer::FileIndex, File*> indexes;
        lexer::FileIndex index = lexer::builtin;

       public:
        std::vector<std::string> file_list() {
            std::vector<std::string> ret;
            ret.reserve(files.size());
            for (size_t i = 1; i <= index; i++) {
                auto u8 = indexes[i]->file_name.generic_u8string();
                ret.push_back(std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size()));
            }
            return ret;
        }

        expected<lexer::FileIndex, std::error_code> add(const auto& name) {
            fs::path path = utils::utf::convert<std::u8string>(name);
            std::error_code err;
            path = fs::canonical(path, err);
            if (err) {
                return unexpect(err);
            }
            if (!fs::is_regular_file(path, err)) {
                return unexpect(err);
            }
            for (auto it = files.begin(); it != files.end(); it++) {
                if (fs::equivalent(it->second.file_name, path, err)) {
                    return unexpect(std::error_code(it->second.file, std::generic_category()));
                }
                else if (err) {
                    return unexpect(err);
                }
            }
            File file;
            file.file_name = std::move(path);
            file.file = index + 1;
            index++;
            auto& f = files[file.file_name];
            f = std::move(file);
            indexes[index] = &f;
            return files.size();
        }

        File* get_input(lexer::FileIndex fd) {
            if (fd < 1 || index < fd) {
                return nullptr;
            }
            auto& file = indexes[fd];
            if (!file->ptr) {
                utils::file::View v;
                if (!v.open(file->file_name.c_str())) {
                    return nullptr;
                }
                file->set_input(std::move(v));
            }
            return file;
        }

        SourceEntry error(std::string&& msg, lexer::Loc loc, bool warn = false) {
            auto got = get_input(loc.file);
            if (!got) {
                return SourceEntry{
                    .msg = std::move(msg),
                    .file = "<unknown source>",
                    .loc = {0, 0},
                    .src = "",
                    .warn = warn,
                };
            }
            return got->error(std::move(msg), loc.pos, warn);
        }
    };

    inline auto to_source_error(FileSet& fs) {
        return [&](LocationError&& err) {
            SourceError src;
            for (auto& loc : err.locations) {
                src.errs.push_back(fs.error(std::move(loc.msg), loc.loc, loc.warn));
            }
            if (err.locations.size() == 0) {
                src.errs.push_back(fs.error("unexpected errors", {}));
            }
            return src;
        };
    }
}  // namespace brgen
