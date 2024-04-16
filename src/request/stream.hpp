/*license*/
#pragma once
#include <file/file_stream.h>
#include "generator.hpp"

namespace brgen::request {
    futils::error::Error<> read_requests(futils::file::File& file, auto&& cb) {
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
