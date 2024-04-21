/*license*/
#pragma once
#include <file/file_stream.h>
#include "generator.hpp"

namespace brgen::request {

    inline futils::error::Error<> write_response_header(futils::binary::writer& w) {
        GeneratorResponseHeader header;
        header.version = 1;
        if (auto err = header.encode(w)) {
            return err;
        }
        return {};
    }

    inline futils::error::Error<> write_response(futils::binary::writer& w, const Response& r) {
        if (auto err = r.encode(w)) {
            return err;
        }
        return {};
    }

    inline futils::error::Error<> write_response(futils::binary::writer& w, SourceCode&& code) {
        Response r;
        r.type = ResponseType::CODE;
        r.source(std::move(code));
        return write_response(w, r);
    }

    inline futils::error::Error<> write_response(futils::binary::writer& w, std::uint64_t id, ResponseStatus status, auto&& name, auto&& src, auto&& warn) {
        Response r;
        r.type = ResponseType::CODE;
        SourceCode code;
        code.id = id;
        code.status = status;
        code.set_error_message(std::forward<decltype(warn)>(warn));
        code.set_name(std::forward<decltype(name)>(name));
        code.set_code(std::forward<decltype(src)>(src));
        return write_response(w, std::move(code));
    }

    inline futils::error::Error<> write_end_response(futils::binary::writer& w, std::uint64_t id) {
        Response r;
        r.type = ResponseType::END_OF_CODE;
        EndOfCode eoc;
        eoc.id = id;
        r.end(std::move(eoc));
        return write_response(w, r);
    }

    inline futils::error::Error<> read_requests(const futils::file::File& file, auto&& cb) {
        futils::file::FileStream<std::string> fs{file};
        futils::binary::reader r{fs.get_read_handler(), &fs};
        GeneratorRequestHeader header;
        r.load_stream(header.fixed_header_size);
        if (auto err = header.decode(r)) {
            return err;
        }
        r.discard();
        for (;;) {
            if (!r.load_stream(1)) {
                break;
            }
            GenerateSource req;
            if (auto err = req.decode(r)) {
                return err;
            }
            if (auto err = cb(req)) {
                return err;
            }
            r.discard();
        }
        return {};
    }

}  // namespace brgen::request
