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

void body_read(futils::fnet::server::Requester&& req, HeaderInfo* hdr, futils::http::body::HTTPBodyInfo info, futils::fnet::flex_storage&& body, futils::fnet::server::BodyState s, futils::fnet::server::StateContext c) {
    auto ptr = std::unique_ptr<HeaderInfo>(hdr);
    std::map<std::string, std::string> response;
    response["Server"] = "src2json (" SRC2JSON_VERSION ")";
    if (s == futils::fnet::server::BodyState::error || s == futils::fnet::server::BodyState::best_effort) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(c, futils::fnet::server::StatusCode::http_bad_request, response, "failed to read body");
        return;
    }
    auto p = futils::json::parse<futils::json::JSON>(body);
    if (p.is_undef()) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(c, futils::fnet::server::StatusCode::http_bad_request, response, "failed to parse json");
        return;
    }
    auto typ = p["args"];
    if (typ.is_undef()) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(c, futils::fnet::server::StatusCode::http_bad_request, response, "args not found");
        return;
    }
    if (!typ.is_array()) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(c, futils::fnet::server::StatusCode::http_bad_request, response, "args is not array");
        return;
    }
    futils::wrap::ArgvVector args;
    for (auto& i : futils::json::as_array(typ)) {
        if (!i.is_string()) {
            response["Connection"] = "close";
            response["Content-Type"] = "text/plain";
            req.respond_flush(c, futils::fnet::server::StatusCode::http_bad_request, response, "args is not array of string");
            return;
        }
        args.push_back(i.force_as_string<std::string>());
    }
    int argc;
    char** argv;
    args.arg(argc, argv);
    is_worker_thread() = true;
    auto res = src2json_main(argc, argv);
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
    if (ptr->keep_alive) {
        futils::fnet::server::handle_keep_alive(std::move(req), std::move(c));
    }
}

void handler(void*, futils::fnet::server::Requester req, futils::fnet::server::StateContext s) {
    auto ptr = std::make_unique<HeaderInfo>();
    bool keep_alive = false;
    futils::http::body::HTTPBodyInfo body_info;
    std::string method;
    std::string path;
    std::map<std::string, std::string> response;
    response["Server"] = "src2json (" SRC2JSON_VERSION ")";
    if (!fnet::server::read_header_and_check_keep_alive<std::string>(req.http, ptr->method, ptr->path, ptr->headers, ptr->keep_alive, &body_info)) {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(s, fnet::server::StatusCode::http_bad_request, response, "failed to read header");
        return;
    }
    if (method == "GET" && method == "/stop") {
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        req.respond_flush(s, fnet::server::StatusCode::http_ok, response, "bye");
        stop = true;
        return;
    }
    if (method != "POST" && path != "/parse") {
        std::map<std::string, std::string> response;
        response["Connection"] = "close";
        response["Content-Type"] = "text/html; charset=utf-8";
        req.respond_flush(s, fnet::server::StatusCode::http_not_found, response, "not found");
        return;
    }
    if (body_info.type == futils::http::body::BodyType::no_info) {
        std::map<std::string, std::string> response;
        response["Connection"] = "close";
        response["Content-Type"] = "text/plain";
        response["Vary"] = "Content-Length,Transfer-Encoding";
        req.respond_flush(s, fnet::server::StatusCode::http_length_required, response, "no body info");
        return;
    }
    fnet::server::read_body(std::move(req), ptr.release(), body_info, std::move(s), body_read);
}

int network_main() {
    fnet::server::HTTPServ serv;
    serv.next = handler;
    auto s = fnet::server::make_state(&serv, fnet::server::http_handler);
    s->set_max_and_active(std::thread::hardware_concurrency() * 2, std::thread::hardware_concurrency() * 2);
    auto p = fnet::server::prepare_listener("8090", 10, false);
    if (!p) {
        print_error("failed to prepare listener: ", p.error());
        return 1;
    }
    cout.set_hook_write(cout_hook);
    cerr.set_hook_write(cerr_hook);
    s->add_accept_thread(std::move(p->first));
    while (!stop) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
