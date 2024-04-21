/*license*/
#pragma once
#include "print.h"
#include <request/stream.hpp>
#include <core/common/util.h>

inline void send_response(brgen::request::SourceCode&& code) {
    futils::file::FileStream<std::string> fs{cout.get_file()};
    futils::binary::writer w{fs.get_write_handler(), &fs};
    auto prev = w.offset();
    if (auto err = brgen::request::write_response(w, std::move(code))) {
        assert(false);  // should not happen
        w.reset(prev);  // reset for consistency
        return;         // ignore error
    }
    // commit by writer handler
}

thread_local bool send_as_text = false;

inline void send_end_response(std::uint64_t id) {
    if (send_as_text) {
        return;
    }
    brgen::request::EndOfCode eoc;
    eoc.id = id;
    futils::file::FileStream<std::string> fs{cout.get_file()};
    futils::binary::writer w{fs.get_write_handler(), &fs};
    if (auto err = brgen::request::write_end_response(w, id)) {
        assert(false);  // should not happen
        w.reset(0);     // reset for consistency
        return;         // ignore error
    }
    // commit by writer handler
}

inline void send_error(std::uint64_t id, auto&&... msg) {
    if (send_as_text) {
        print_error(msg...);
        return;
    }
    auto err_msg = brgen::concat(msg...);
    brgen::request::SourceCode code;
    code.id = id;
    code.status = brgen::request::ResponseStatus::ERROR;
    code.set_error_message(err_msg);
    send_response(std::move(code));
}

inline void send_error_and_end(std::uint64_t id, auto&&... msg) {
    send_error(id, msg...);
    send_end_response(id);
}

inline void send_source(std::uint64_t id, std::string&& src, const auto&& name) {
    if (send_as_text) {
        cout << src << "\n";
        return;
    }
    brgen::request::SourceCode code;
    code.id = id;
    code.status = brgen::request::ResponseStatus::OK;
    code.set_code(std::move(src));
    code.set_name(name);
    send_response(std::move(code));
}

inline void send_response_header() {
    if (send_as_text) {
        return;
    }
    futils::file::FileStream<std::string> fs{cout.get_file()};
    futils::binary::writer w{fs.get_write_handler(), &fs};
    if (auto err = brgen::request::write_response_header(w)) {
        assert(false);  // should not happen
        w.reset(0);     // reset for consistency
        return;         // ignore error
    }
    // commit by writer handler
}

inline void read_stdin_requests(auto&& cb) {
    if (send_as_text) {
        return;
    }
    send_response_header();
    if (auto err = brgen::request::read_requests(futils::file::File::stdin_file(), cb)) {
        send_error_and_end(0, "cannot read requests: ", err.template error<std::string>());
    }
}