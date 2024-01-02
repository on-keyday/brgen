/*license*/
#include <fnet/server/state.h>
#include <fnet/server/httpserv.h>
#include <wrap/cout.h>
#include <thread>
#include <set>
#include "../common/print.h"
#include <map>
#include <fnet/html.h>
#include <atomic>
#include "version.h"
#include <json/json_export.h>
#include <json/to_string.h>
#include <json/iterator.h>
#include <wrap/argv.h>
#include <helper/lock.h>
namespace fnet = futils::fnet;

std::atomic_bool stop = false;

bool& is_worker_thread() {
    static thread_local bool is_worker = false;
    return is_worker;
}

std::string& worker_stdout_buffer() {
    static thread_local std::string buffer;
    return buffer;
}

std::string& worker_stderr_buffer() {
    static thread_local std::string buffer;
    return buffer;
}

futils::file::file_result<void> cout_hook(futils::wrap::UtfOut& out, futils::view::rvec v) {
    if (is_worker_thread()) {
        worker_stdout_buffer().append(v.as_char(), v.size());
        return {};
    }
    return out.write_no_hook(v);
}

futils::file::file_result<void> cerr_hook(futils::wrap::UtfOut& out, futils::view::rvec v) {
    if (is_worker_thread()) {
        worker_stderr_buffer().append(v.as_char(), v.size());
        return {};
    }
    return out.write_no_hook(v);
}

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
            arg += "<source code...>";
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
    auto res = src2json_main(argc, argv, true);
    is_worker_thread() = false;
    auto stdout_buffer = std::move(worker_stdout_buffer());
    auto stderr_buffer = std::move(worker_stderr_buffer());
    p["stdout"] = std::move(stdout_buffer);
    p["stderr"] = std::move(stderr_buffer);
    p["exit_code"] = res;
    auto text = futils::json::to_string<std::string>(p);
    response["Connection"] = ptr->keep_alive ? "keep-alive" : "close";
    response["Content-Type"] = "application/json";
    req.respond_flush(c, futils::fnet::server::StatusCode::http_ok, response, text);
    c.log(futils::fnet::server::log_level::info, req.client.addr, ptr->method, " ", ptr->path, " -> ", futils::number::to_string<std::string>(int(futils::fnet::server::StatusCode::http_ok)));
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
    if (ptr->method == "GET" && ptr->path == "/stop") {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(s, fnet::server::StatusCode::http_ok, response, "bye");
        stop = true;
        return;
    }
    if (ptr->method != "POST" || ptr->path != "/parse") {
        error_response(fnet::server::StatusCode::http_not_found, "not found");
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
    fnet::server::log_level level;
    fnet::NetAddrPort addr;
    fnet::error::Error err;
};

std::mutex log_mutex;
std::deque<LogEntry> log_buffer;

void logger(fnet::server::log_level level, fnet::NetAddrPort* addr, fnet::error::Error& err) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_buffer.push_back({level, addr ? *addr : fnet::NetAddrPort{}, err});
}

int network_main(const char* port) {
    fnet::server::HTTPServ serv;
    serv.next = handler;
    auto s = fnet::server::make_state(&serv, fnet::server::http_handler);
    s->set_log(logger);
    s->set_max_and_active(std::thread::hardware_concurrency() * 2, std::thread::hardware_concurrency() * 2);
    auto p = fnet::server::prepare_listener(port, 10, false);
    if (!p) {
        print_error("failed to prepare listener: ", p.error());
        return exit_err;
    }
    cout.set_hook_write(cout_hook);
    cerr.set_hook_write(cerr_hook);
    s->add_accept_thread(std::move(p->first));
    print_note("listening on ", p->second.addr.to_string<std::string>());
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
            auto addr = d.addr.to_string<std::string>();
            if (level == fnet::server::log_level::err) {
                print_error(addr, ": ", d.err.error());
            }
            else if (level == fnet::server::log_level::warn) {
                print_warning(addr, ": ", d.err.error());
            }
            else if (level == fnet::server::log_level::info) {
                print_note(addr, ": ", d.err.error());
            }
            else if (level == fnet::server::log_level::debug) {
                print_note(addr, ": [debug] ", d.err.error());
            }
        }
    }
    s->notify();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return exit_ok;
}
