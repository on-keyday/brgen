// Created by make_cpp_test.sh
#include "stub.hpp"
#include <wrap/cout.h>
#include <file/file_view.h>
#include <binary/reader.h>
#include <binary/writer.h>

int main(int argc, char** argv) {
    auto& cout = futils::wrap::cout_wrap();
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <file>\n";
        return 1;
    }
    TEST_CLASS t;
    futils::file::View view;
    if (!view.open(argv[1])) {
        cout << "Failed to open file " << argv[1] << '\n';
        return 1;
    }
    if (!view.data()) {
        cout << "Failed to read file " << argv[1] << '\n';
        return 1;
    }
    futils::binary::reader r{view};
    if (!t.decode(r)) {
        cout << "Failed to decode file " << argv[1] << "; decode() returns false\n";
        return 1;
    }
    if (!r.empty()) {
        cout << "Failed to decode file " << argv[1] << ";" << r.remain().size() << " bytes left\n";
        return 1;
    }
    std::string s;
    s.resize(view.size());
    futils::binary::writer w{s};
    if (!t.encode(w)) {
        cout << "Failed to encode file " << argv[1] << "; encode() returns false\n";
        return 1;
    }
    if (!w.full()) {
        cout << "Failed to encode file " << argv[1] << ";" << w.remain().size() << " bytes left\n";
        return 1;
    }
    if (futils::view::rvec(view) != s) {
        cout << "Failed to encode file " << argv[1] << "; encoded data is different\n";
        return 1;
    }
    view.close();
    for (auto i = 0; i < 10000; i++) {
        futils::binary::reader r{s};
        if (!t.decode(r)) {
            cout << "Failed to decode repeat test " << argv[1] << "; decode() returns false\n";
            return 1;
        }
        futils::binary::writer w{s};
        if (!t.encode(w)) {
            cout << "Failed to encode repeat test " << argv[1] << "; encode() returns false\n";
            return 1;
        }
    }
    return 0;
}
