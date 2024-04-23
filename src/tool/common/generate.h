/*license*/
#pragma once
#include "load_json.h"
#include <filesystem>
#include <file/file_view.h>

template <class T>
int do_generate(const auto& flags, brgen::request::GenerateSource& req, T&& input, auto&& cb) {
    auto res = load_json(req.id, req.name, std::forward<T>(input));
    if (!res) {
        return 1;
    }
    return cb(flags, req, res);
}

int generate_from_file(const auto& flags, auto&& cb) {
    if (flags.args.empty()) {
        print_error("no input file");
        return 1;
    }
    if (flags.args.size() > 1) {
        print_error("too many input files");
        return 1;
    }
    auto name = flags.args[0];
    futils::file::View view;
    if (!view.open(name)) {
        print_error("cannot open file ", name);
        return 1;
    }
    std::filesystem::path path{name};
    auto u8name = path.filename().replace_extension().generic_u8string();
    brgen::request::GenerateSource req;
    req.id = 0;
    req.name = std::string(reinterpret_cast<const char*>(u8name.data()), u8name.size());
    send_as_text = true;
    return do_generate(flags, req, view, cb);
}
