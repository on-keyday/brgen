/*license*/
#pragma once
#include "print.h"
#include <request/stream.hpp>
#include <core/common/util.h>
#if __has_include(<thread>) && __has_include(<mutex>)
#include <thread>
#include <thread/channel.h>
#include <mutex>
#define HAS_THREAD
#endif

inline void do_send(auto&& cb) {
#ifdef HAS_THREAD
    static std::mutex send_mutex;
    futils::file::FileStream<std::string, const futils::file::File&, std::mutex&> fs{cout.get_file(), send_mutex};
#else
    futils::file::FileStream<std::string> fs{cout.get_file()};
#endif

    futils::binary::writer w{fs.get_write_handler(), &fs};
    auto prev = w.offset();
    if (auto err = cb(w)) {
        assert(false);  // should not happen
        w.reset(prev);  // reset for consistency
        return;         // ignore error
    }
    // commit by writer handler
}

inline void send_response(brgen::request::SourceCode&& code) {
    return do_send([&](futils::binary::writer& w) -> futils::error::Error<> {
        return brgen::request::write_response(w, std::move(code));
    });
}

thread_local bool send_as_text = false;

inline void send_end_response(std::uint64_t id) {
    if (send_as_text) {
        return;
    }
    return do_send([&](futils::binary::writer& w) -> futils::error::Error<> {
        return brgen::request::write_end_response(w, id);
    });
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

inline void send_source(std::uint64_t id, std::string&& src, const auto& name) {
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

inline void send_source_and_end(std::uint64_t id, std::string&& src, const auto& name) {
    send_source(id, std::move(src), name);
    send_end_response(id);
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

inline void read_stdin_requests_impl(auto&& cb) {
    if (send_as_text) {
        return;
    }
    send_response_header();  // here not multi-thread
    if (auto err = brgen::request::read_requests(futils::file::File::stdin_file(), cb)) {
        send_error_and_end(0, "cannot read requests: ", err.template error<std::string>());
    }
}

#ifdef HAS_THREAD
inline void read_stdin_requests(auto&& cb) {
    // TODO(on-keyday): because some compiler not capture structured binding,
    //                  so we use `auto send_recv` instead of `auto [send,recv]`
    auto send_recv = futils::thread::make_chan<brgen::request::GenerateSource>();
    auto& send = send_recv.first;
    auto& recv = send_recv.second;
    auto thread_handler = [&](futils::thread::RecvChan<brgen::request::GenerateSource> recv) {
        recv.set_blocking(false);
        while (true) {
            brgen::request::GenerateSource req;
            auto s = recv >> req;
            if (s == futils::thread::ChanStateValue::closed) {
                break;
            }
            if (s != futils::thread::ChanStateValue::enable) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            cb(req);
        }
    };
    std::vector<std::thread> threads;
    for (size_t i = 0; i < std::thread::hardware_concurrency() / 2; i++) {
        threads.emplace_back(thread_handler, recv);
    }
    read_stdin_requests_impl([&](brgen::request::GenerateSource& req) {
        send.set_blocking(true);
        send << std::move(req);
        return futils::error::Error<>{};
    });
    send.close();
    for (auto& t : threads) {
        t.join();
    }
}
#else
inline void read_stdin_requests(auto&& cb) {
    read_stdin_requests_impl(cb);
}
#endif
