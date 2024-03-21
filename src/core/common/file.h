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
#include <unicode/utf/view.h>
#include <binary/view.h>

namespace brgen {
    namespace fs = std::filesystem;

    struct File {
       private:
        friend struct FileSet;
        template <class Buf, class T>
        friend bool make_file_from_text(File& file, T&& t);

        template <class TokenBuf, class T>
        static std::optional<lexer::Token> do_parse(void* ptr, std::uint64_t file) {
            return lexer::parse_one<TokenBuf>(*static_cast<futils::Sequencer<T>*>(ptr), file);
        }

        template <class DumpBuf, class T>
        static std::pair<std::string, futils::code::SrcLoc> dump_source(void* ptr, lexer::Pos pos) {
            auto& seq = *static_cast<futils::Sequencer<T>*>(ptr);
            size_t rptr = seq.rptr;
            seq.rptr = pos.begin;
            DumpBuf out;
            auto loc = futils::code::write_src_loc(out, seq, pos.len());
            seq.rptr = rptr;
            if constexpr (std::is_same_v<DumpBuf, std::string>) {
                return {std::move(out), loc};
            }
            else {
                return {futils::utf::convert<std::string>(out), loc};
            }
        }

        lexer::FileIndex file = lexer::builtin;
        bool special = false;
        fs::path file_name;
        std::shared_ptr<void> ptr;
        std::optional<lexer::Token> (*parse_)(void* seq, std::uint64_t file) = nullptr;
        std::pair<std::string, futils::code::SrcLoc> (*dump_)(void* seq, lexer::Pos pos) = nullptr;

        futils::view::rvec (*direct)(void* seq) = nullptr;

        template <class T>
        static futils::view::rvec direct_source(void* p) {
            auto& seq = *static_cast<futils::Sequencer<T>*>(p);
            if constexpr (std::is_convertible_v<T, futils::view::rvec>) {
                return seq.buf.buffer;
            }
            return {};
        }

        template <class Buf, class T>
        void set_input(T&& t) {
            using U = std::decay_t<T>;
            ptr = std::make_unique<futils::Sequencer<U>>(std::forward<T>(t));
            parse_ = do_parse<Buf, U>;
            dump_ = dump_source<Buf, U>;
            direct = direct_source<U>;
        }

       public:
        File() = default;

        File(lexer::FileIndex fd, std::string&& name) {
            file = fd;
            file_name = std::move(name);
        }

        File(const File&) = delete;

        constexpr bool is_special() const {
            return special;
        }

        std::optional<lexer::Token> parse() {
            if (parse_) {
                return parse_(ptr.get(), file);
            }
            return std::nullopt;
        }

       private:
        std::pair<std::string, futils::code::SrcLoc> dump(lexer::Pos pos) {
            if (dump_) {
                return dump_(ptr.get(), pos);
            }
            return {};
        }

       public:
        SourceEntry error(std::string&& msg, lexer::Loc loc, bool warn = false) {
            auto [src, _] = dump(loc.pos);
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

        size_t index() const {
            return file;
        }

        futils::view::rvec source() const {
            if (direct) {
                return direct(ptr.get());
            }
            return {};
        }
    };

    template <class Buf, class T>
    bool make_file_from_text(File& file, T&& t) {
        file.set_input<Buf>(std::forward<T>(t));
        return true;
    }

    enum class UtfMode {
        utf8,
        utf16,
        utf16le,
        utf16be,
        utf32,
        utf32le,
        utf32be,
    };

    struct FileSet {
       private:
        std::map<fs::path, File> files;
        std::map<lexer::FileIndex, File*> indexes;
        lexer::FileIndex index = lexer::builtin;
        bool utf16_mode = false;
        bool file_as_utf16 = false;
        UtfMode input_mode = UtfMode::utf8;
        UtfMode interpret_mode = UtfMode::utf8;

        void set_input_with_mode(File& f, auto&& buffer) {
            if (input_mode == interpret_mode) {
                switch (input_mode) {
                    case UtfMode::utf8:
                        assert(sizeof(buffer[1]) == 1);
                        make_file_from_text<std::string>(f, std::forward<decltype(buffer)>(buffer));
                        break;
                    case UtfMode::utf16:
                        assert(sizeof(buffer[1]) == 2);
                        make_file_from_text<std::u16string>(f, std::forward<decltype(buffer)>(buffer));
                        break;
                    case UtfMode::utf32:
                        assert(sizeof(buffer[1]) == 4);
                        make_file_from_text<std::u32string>(f, std::forward<decltype(buffer)>(buffer));
                        break;
                    default:
                        assert(false);
                        __builtin_unreachable();
                }
                return;
            }
            else if (input_mode == UtfMode::utf8) {
                assert(sizeof(buffer[1]) == 1);
                switch (interpret_mode) {
                    case UtfMode::utf16:
                        make_file_from_text<std::u16string>(f, futils::utf::U16View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    case UtfMode::utf32:
                        make_file_from_text<std::u32string>(f, futils::utf::U32View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    default:
                        assert(false);
                        __builtin_unreachable();
                }
                return;
            }
            else if (input_mode == UtfMode::utf16le || input_mode == UtfMode::utf16be) {
                assert(sizeof(buffer[1]) == 2);
                switch (interpret_mode) {
                    case UtfMode::utf8:
                        make_file_from_text<std::string>(f, futils::utf::U8View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    case UtfMode::utf32:
                        make_file_from_text<std::u32string>(f, futils::utf::U32View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    default:
                        assert(false);
                        __builtin_unreachable();
                }
                return;
            }
            else if (input_mode == UtfMode::utf32le || input_mode == UtfMode::utf32be) {
                assert(sizeof(buffer[1]) == 4);
                switch (interpret_mode) {
                    case UtfMode::utf8:
                        make_file_from_text<std::string>(f, futils::utf::U8View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    case UtfMode::utf16:
                        make_file_from_text<std::u16string>(f, futils::utf::U16View<std::decay_t<decltype(buffer)>>(std::forward<decltype(buffer)>(buffer)));
                        break;
                    default:
                        assert(false);
                        __builtin_unreachable();
                }
                return;
            }
            assert(false);
            __builtin_unreachable();
        }

        void set_file_with_input_mode(File& file, auto&& view) {
            assert(sizeof(view[1]) == 1);
            using View = std::decay_t<decltype(view)>;
            using futils::binary::EndianView;
            if (input_mode == UtfMode::utf8) {
                set_input_with_mode(file, std::move(view));
            }
            else if (input_mode == UtfMode::utf16be) {
                set_input_with_mode(file, EndianView<View, char16_t>(std::move(view), false));
            }
            else if (input_mode == UtfMode::utf16le) {
                set_input_with_mode(file, EndianView<View, char16_t>(std::move(view), true));
            }
            else if (input_mode == UtfMode::utf32be) {
                set_input_with_mode(file, EndianView<View, char32_t>(std::move(view), false));
            }
            else if (input_mode == UtfMode::utf32le) {
                set_input_with_mode(file, EndianView<View, char32_t>(std::move(view), true));
            }
            else {
                assert(false);
                __builtin_unreachable();
            }
        }

       public:
        void
        set_utf_mode(UtfMode input_mode, UtfMode interpret_mode) {
            this->input_mode = input_mode;
            this->interpret_mode = interpret_mode;
        }

        std::vector<std::string> file_list() {
            std::vector<std::string> ret;
            ret.reserve(files.size());
            for (size_t i = 1; i <= index; i++) {
                auto u8 = indexes[i]->file_name.generic_u8string();
                ret.push_back(std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size()));
            }
            return ret;
        }

        expected<lexer::FileIndex, std::error_code> add_special(const auto& name, auto&& buffer) {
            fs::path path = futils::utf::convert<std::u8string>(name);
            if (files.find(path) != files.end()) {
                return unexpect(std::make_error_code(std::errc::file_exists));
            }
            File file;
            file.file_name = std::move(path);
            file.file = index + 1;
            file.special = true;
            index++;
            auto& f = files[file.file_name];
            f = std::move(file);
            indexes[index] = &f;
            set_file_with_input_mode(f, std::forward<decltype(buffer)>(buffer));
            return files.size();
        }

        expected<lexer::FileIndex, std::error_code> add_file(const auto& name, bool allow_duplicate = false, fs::path base_path = {}) {
            fs::path path = futils::utf::convert<std::u8string>(name);
            if (!base_path.empty() && path.is_relative()) {
                path = base_path / path;
            }
            std::error_code err;
            path = fs::canonical(path, err);
            if (err) {
                return unexpect(err);
            }
            if (!fs::is_regular_file(path, err)) {
                return unexpect(err);
            }
            for (auto it = files.begin(); it != files.end(); it++) {
                if (it->second.special) {
                    continue;
                }
                if (fs::equivalent(it->second.file_name, path, err)) {
                    if (allow_duplicate) {
                        return it->second.file;
                    }
                    return unexpect(std::error_code(int(std::errc::file_exists), std::generic_category()));
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

        fs::path get_path(lexer::FileIndex fd) {
            if (fd < 1 || index < fd) {
                return {};
            }
            return indexes[fd]->file_name;
        }

        File* get_input(lexer::FileIndex fd) {
            if (fd < 1 || index < fd) {
                return nullptr;
            }
            auto& file = indexes[fd];
            if (!file->ptr) {
                futils::file::View v;
                if (!v.open(file->file_name.c_str())) {
                    return nullptr;
                }
                set_file_with_input_mode(*file, std::move(v));
            }
            return file;
        }

        SourceEntry error(auto&& msg, lexer::Loc loc, bool warn = false) {
            auto got = get_input(loc.file);
            if (!got) {
                return SourceEntry{
                    .msg = std::forward<decltype(msg)>(msg),
                    .file = "<unknown source>",
                    .loc = {0, 0},
                    .src = "",
                    .warn = warn,
                };
            }
            return got->error(std::forward<decltype(msg)>(msg), loc, warn);
        }
    };

    inline auto to_source_error(FileSet& fs) {
        return [&](const LocationError& err) {
            SourceError src;
            for (auto& loc : err.locations) {
                src.errs.push_back(fs.error(loc.msg, loc.loc, loc.warn));
            }
            if (err.locations.size() == 0) {
                src.errs.push_back(fs.error("unexpected errors", {}));
            }
            return src;
        };
    }

    inline std::string to_error_message(std::error_code ec) {
        std::string val;
        futils::file::format_os_error(val, ec.value());
        std::erase_if(val, [](char c) { return c == '\n' || c == '\r'; });
        return concat("code=", ec.category().name(), ":", nums(ec.value()), ":", val);
    }
}  // namespace brgen
