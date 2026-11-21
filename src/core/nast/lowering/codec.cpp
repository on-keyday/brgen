/*license*/
#include "codec.hpp"
#include "stream_io.hpp"

namespace brgen::nast::lowering {

    CodecParams codec_params(Context& c, Node<Format> fmt, Direction dir) {
        CodecParams out;
        auto loc = fmt ? fmt.ref(c.a).loc() : lexer::Loc{};
        out.stream = dir == Direction::decode ? input_stream(c, loc) : output_stream(c, loc);
        if (!fmt) {
            return out;
        }
        if (auto* req = c.tables.table<Requirements>().get(fmt)) {
            if (dir == Direction::decode) {
                out.read = req->decode_state_read;
                out.write = req->decode_state_write;
            }
            else {
                out.read = req->encode_state_read;
                out.write = req->encode_state_write;
            }
        }
        return out;
    }

}  // namespace brgen::nast::lowering
