/*license*/
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cout.h>
#include <thread>
#include <set>
#include "../common/print.h"
#include <map>
#include <atomic>
#ifdef SRC2JSON_DLL
#include "capi_export.h"
#endif
#include "version.h"
#include <json/json_export.h>
#include <json/to_string.h>
#include <json/iterator.h>
#include <wrap/argv.h>
#include <helper/lock.h>
#include <fnet/util/uri.h>
#include <mutex>
#include <fnet/server/format_state.h>
#include <timer/clock.h>
#include <timer/to_string.h>
#include "hook.h"
namespace fnet = futils::fnet;

std::atomic_bool stop = false;

struct HeaderInfo {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    bool keep_alive = false;
};

auto error_responder(std::map<std::string, std::string>& response, std::string& method, std::string& path, futils::fnet::server::Requester& req, futils::fnet::server::StateContext& s) {
    return [&](futils::fnet::server::StatusCode code, std::string_view msg) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(s, code, response, msg);
        s.log(futils::fnet::server::log_level::warn, req.client.addr, method, " ", path, " -> ", futils::number::to_string<std::string>(int(code)), ": ", msg);
    };
}

static bool unsafe_escape = false;
static Capability global_cap;

void body_read(futils::fnet::server::Requester&& req, HeaderInfo* hdr, futils::http::body::HTTPBodyInfo info, futils::fnet::flex_storage&& body, futils::fnet::server::BodyState s, futils::fnet::server::StateContext c) {
    auto ptr = std::unique_ptr<HeaderInfo>(hdr);
    std::map<std::string, std::string> response;
    response["Server"] = "src2json (" SRC2JSON_VERSION ")";
    auto error_response = error_responder(response, hdr->method, hdr->path, req, c);
    if (s == futils::fnet::server::BodyState::error || s == futils::fnet::server::BodyState::best_effort) {
        error_response(futils::fnet::server::StatusCode::http_bad_request, "failed to read body");
        return;
    }
    auto p = futils::json::parse<futils::json::JSON>(body);
    if (p.is_undef()) {
        error_response(futils::fnet::server::StatusCode::http_bad_request, "failed to parse json");
        return;
    }
    auto typ = p["args"];
    if (typ.is_undef()) {
        error_response(futils::fnet::server::StatusCode::http_bad_request, "failed to get args");
        return;
    }
    if (!typ.is_array()) {
        error_response(futils::fnet::server::StatusCode::http_bad_request, "args is not array");
        return;
    }
    futils::wrap::ArgvVector args;
    std::string arg = "src2json";
    args.push_back("src2json");
    for (auto& i : futils::json::as_array(typ)) {
        if (!i.is_string()) {
            error_response(futils::fnet::server::StatusCode::http_bad_request, "args is not array of string");
            return;
        }
        auto flag = i.force_as_string<std::string>();
        arg += " ";
        if (flag.find('\n') != std::string::npos) {
            arg += "<multiline text...>";
        }
        else if (flag.find(' ') != std::string::npos) {
            arg += "\"";
            arg += flag;
            arg += "\"";
        }
        else {
            arg += flag;
        }
        args.push_back(std::move(flag));
    }
    int argc;
    char** argv;
    args.arg(argc, argv);
    c.log(futils::fnet::server::log_level::info, req.client.addr, "executing: ", arg);
    is_worker_thread() = true;
    auto res = src2json_main(argc, argv, global_cap);
    is_worker_thread() = false;
    auto stdout_buffer = std::move(worker_stdout_buffer());
    auto stderr_buffer = std::move(worker_stderr_buffer());
    futils::json::Stringer st;
    if (!unsafe_escape) {
        st.set_utf_escape(true, false, true, true);
    }
    {
        auto field = st.object();
        field("stdout", stdout_buffer);
        field("stderr", stderr_buffer);
        field("exit_code", res);
        field("args", args.holder_);
    }
    auto text = st.out();
    response["Connection"] = ptr->keep_alive ? "keep-alive" : "close";
    response["Content-Type"] = "application/json";
    auto status_code = futils::fnet::server::StatusCode::http_ok;
    req.respond_flush(c, status_code, response, text);
    auto level = futils::fnet::server::log_level::info;
    c.log(level, req.client.addr, ptr->method, " ", ptr->path, " -> ", futils::number::to_string<std::string>(int(status_code)));
    if (ptr->keep_alive) {
        futils::fnet::server::handle_keep_alive(std::move(req), std::move(c));
    }
}

void handler(void*, futils::fnet::server::Requester req, futils::fnet::server::StateContext s) {
    auto ptr = std::make_unique<HeaderInfo>();
    bool keep_alive = false;
    futils::http::body::HTTPBodyInfo body_info;
    std::map<std::string, std::string> response;
    response["Server"] = "src2json (" SRC2JSON_VERSION ")";
    auto error_response = error_responder(response, ptr->method, ptr->path, req, s);
    if (!fnet::server::read_header_and_check_keep_alive<std::string>(req.http, ptr->method, ptr->path, ptr->headers, ptr->keep_alive, &body_info)) {
        error_response(fnet::server::StatusCode::http_bad_request, "failed to read header");
        return;
    }
    futils::uri::URI<std::string> uri;
    if (futils::uri::parse_ex(uri, ptr->path, false, false) != futils::uri::ParseError::none) {
        error_response(fnet::server::StatusCode::http_bad_request, "failed to parse uri");
        return;
    }
    if (ptr->method == "GET" && uri.path == "/stat") {
        response["Connection"] = "close";
        response["Content-Type"] = "application/json";
        req.respond_flush(s, fnet::server::StatusCode::http_ok, response, fnet::server::format_state<std::string>(s.state()));
        s.log(fnet::server::log_level::info, req.client.addr, ptr->method, " ", ptr->path, " -> ", futils::number::to_string<std::string>(int(fnet::server::StatusCode::http_ok)));
        return;
    }
    if (ptr->method == "GET" && uri.path == "/stop") {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(s, fnet::server::StatusCode::http_ok, response, "bye");
        s.log(fnet::server::log_level::info, req.client.addr, ptr->method, " ", ptr->path, " -> ", futils::number::to_string<std::string>(int(fnet::server::StatusCode::http_ok)), ": bye");
        stop = true;
        return;
    }
    if (ptr->method != "POST" || uri.path != "/parse") {
        error_response(fnet::server::StatusCode::http_not_found, "not found");
        return;
    }
    if (ptr->headers["Content-Type"] != "application/json") {
        error_response(fnet::server::StatusCode::http_bad_request, "Content-Type must be application/json");
        return;
    }
    if (body_info.type == futils::http::body::BodyType::no_info) {
        response["Vary"] = "Content-Length,Transfer-Encoding";
        error_response(fnet::server::StatusCode::http_length_required, "length required");
        return;
    }
    fnet::server::read_body(std::move(req), ptr.release(), body_info, std::move(s), body_read);
}

struct LogEntry {
    futils::timer::Time time;
    fnet::server::log_level level;
    fnet::NetAddrPort addr;
    std::string err;
};

std::mutex log_mutex;
std::deque<LogEntry> log_buffer;

void logger(fnet::server::log_level level, fnet::NetAddrPort* addr, fnet::error::Error& err) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_buffer.push_back({futils::timer::utc_clock.now(), level, addr ? *addr : fnet::NetAddrPort{}, err.error()});
}

auto time_str(futils::timer::Time t) {
    return futils::timer::to_string<std::string>("%Y-%M-%D %h:%m:%s.%n", t);
}

int network_main(const char* port, bool unsafe, const Capability& cap) {
    unsafe_escape = unsafe;
    global_cap = cap;
    global_cap.network = false;
    fnet::server::HTTPServ serv;
    serv.next = handler;
    auto s = fnet::server::make_state(&serv, fnet::server::http_handler);
    s->set_log(logger);
    s->set_max_and_active(std::thread::hardware_concurrency() * 2, std::thread::hardware_concurrency() * 2);
    auto p = fnet::server::prepare_listener(port, 1000, false);
    if (!p) {
        print_error("failed to prepare listener: ", p.error());
        return exit_err;
    }
    if (!cout.get_hook_write()) {
        cout.set_hook_write(cout_hook);
        cerr.set_hook_write(cerr_hook);
    }
    s->add_accept_thread(std::move(p->first));
    print_note(time_str(futils::timer::utc_clock.now()),
               ": listening on ",
               p->second.addr.to_string<std::string>());
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto d = futils::helper::lock(log_mutex);
        if (log_buffer.empty()) {
            continue;
        }
        auto buf = std::move(log_buffer);
        log_buffer.clear();
        d.execute();  // unlock here
        for (auto d : buf) {
            auto level = d.level;
            auto prefix = time_str(d.time);
            prefix += " ";
            prefix += d.addr.to_string<std::string>();
            if (level == fnet::server::log_level::err) {
                print_error(prefix, ": ", d.err);
            }
            else if (level == fnet::server::log_level::warn) {
                print_warning(prefix, ": ", d.err);
            }
            else if (level == fnet::server::log_level::info) {
                print_note(prefix, ": ", d.err);
            }
            else if (level == fnet::server::log_level::debug) {
                print_note(prefix, ": [debug] ", d.err);
            }
        }
    }
    s->notify();
    print_note(time_str(futils::timer::utc_clock.now()), ": requesting shutdown...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    print_note(time_str(futils::timer::utc_clock.now()), ": exiting...");
    return exit_ok;
}
