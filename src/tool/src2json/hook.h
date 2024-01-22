/*license*/
#pragma once
#include "../common/print.h"

inline bool& is_worker_thread() {
    static thread_local bool is_worker = false;
    return is_worker;
}

inline futils::wrap::write_hook_fn& worker_hook() {
    static thread_local futils::wrap::write_hook_fn hook = nullptr;
    return hook;
}

inline std::string& worker_stdout_buffer() {
    static thread_local std::string buffer;
    return buffer;
}

inline std::string& worker_stderr_buffer() {
    static thread_local std::string buffer;
    return buffer;
}

inline futils::file::file_result<void> cout_hook(futils::wrap::UtfOut& out, futils::view::rvec v) {
    if (is_worker_thread()) {
        worker_stdout_buffer().append(v.as_char(), v.size());
        return {};
    }
    if (worker_hook()) {
        return worker_hook()(out, v);
    }
    return out.write_no_hook(v);
}

inline futils::file::file_result<void> cerr_hook(futils::wrap::UtfOut& out, futils::view::rvec v) {
    if (is_worker_thread()) {
        worker_stderr_buffer().append(v.as_char(), v.size());
        return {};
    }
    if (worker_hook()) {
        return worker_hook()(out, v);
    }
    return out.write_no_hook(v);
}