// Created by make_cpp_test.sh
#include "stub.hpp"
#include <wrap/cout.h>
#include <file/file_view.h>
#include <binary/reader.h>
#include <binary/writer.h>
#include <fstream>
auto& cout = futils::wrap::cerr_wrap();

int do_encode(char** argv, auto& t, auto& w) {
    if constexpr (std::is_same_v<decltype(t.encode(w)), bool>) {
        if (!t.encode(w)) {
            cout << "Failed to encode file " << argv[1] << "; encode() returns false\n";
            return 2;
        }
    }
    else {
        if (auto err = t.encode(w)) {
            cout << "Failed to encode file " << argv[1] << "; encode() returns " << err.template error<std::string>() << '\n';
            return 2;
        }
    }
    return 0;
}

int do_decode(char** argv, auto& t, auto& r) {
    if constexpr (std::is_same_v<decltype(t.decode(r)), bool>) {
        if (!t.decode(r)) {
            cout << "Failed to decode file " << argv[1] << "; decode() returns false\n";
            return 1;
        }
    }
    else {
        if (auto err = t.decode(r)) {
            cout << "Failed to decode file " << argv[1] << "; decode() returns " << err.template error<std::string>() << '\n';
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cout << "Usage: " << argv[0] << " <file>\n";
        return 2;
    }
    TEST_CLASS t;
    futils::file::View view;
    if (!view.open(argv[1])) {
        cout << "Failed to open file " << argv[1] << '\n';
        return 2;
    }
    if (!view.data()) {
        cout << "Failed to read file " << argv[1] << '\n';
        return 2;
    }
    futils::binary::reader r{view};
    if (auto e = do_decode(argv, t, r)) {
        return e;
    }
    if (!r.empty()) {
        cout << "Failed to decode file " << argv[1] << ";" << r.remain().size() << " bytes left\n";
        return 1;
    }
    std::string s;
    s.resize(view.size());
    futils::binary::writer w{futils::binary::resizable_buffer_writer<std::string>(), &s};
    if (auto e = do_encode(argv, t, w)) {
        return e;
    }
    std::ofstream fs(argv[2], std::ios::binary | std::ios::out);
    if (!fs) {
        cout << "Failed to open file " << argv[2] << '\n';
        return 2;
    }
    fs << s;
}
