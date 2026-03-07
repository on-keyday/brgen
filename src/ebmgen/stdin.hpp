/*license*/
#pragma once
#include <file/file.h>
#include <optional>
#include <variant>
#include "wrap/cin.h"

namespace ebmgen {
    struct Stdin {
        std::variant<std::monostate, futils::file::MMap, std::string> mmapped_input;
        std::optional<futils::view::rvec> stdin_data;
        futils::file::file_result<void> try_read_stdin() {
            auto& in = futils::wrap::cin_wrap().get_file();
            // try mmap first
            auto mmapped = in.mmap(futils::file::r_perm);
            if (!mmapped) {
                // this means input is not seekable, read all data
                std::string data;
                auto ok = in.read_all([&](auto input) {
                    data.append((const char*)input.data(), input.size());
                    return true;
                });
                if (!ok) {
                    return ok;
                }
                mmapped_input = std::move(data);
                stdin_data = std::get<std::string>(mmapped_input);
            }
            else {
                mmapped_input = std::move(*mmapped);
                stdin_data = std::get<futils::file::MMap>(mmapped_input).read_view();
            }
            return {};
        }
    };
}  // namespace ebmgen