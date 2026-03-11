/*license*/
#include <bmgen/internal.hpp>

namespace rebgn {
    // this function binds encoder and decoder to format
    // `bind` means to place function declaration (DECLARE_FUNCTION) and DEFINE_ENCODER or DEFINE_DECODER in range between DEFINE_FORMAT and END_FORMAT
    Error bind_encoder_and_decoder(Module& m) {
        std::vector<Code> rebound;
        std::unordered_map<std::uint64_t, std::uint64_t> should_bind_encoder;
        std::unordered_map<std::uint64_t, std::uint64_t> should_bind_decoder;
        std::unordered_map<std::uint64_t, std::uint64_t> encoder_to_fmt;
        std::unordered_map<std::uint64_t, std::uint64_t> decoder_to_fmt;
        std::unordered_map<std::uint64_t, std::uint64_t> fmt_to_encoder;
        std::unordered_map<std::uint64_t, std::uint64_t> fmt_to_decoder;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                auto fmt_begin = c.left_ref().value().value();
                auto found = m.ident_index_table.find(fmt_begin);
                if (found == m.ident_index_table.end()) {
                    return error("Invalid format ident: {}", fmt_begin);
                }
                bool has_declare_in_fmt = false;
                for (auto fmt = found->second; fmt < m.code.size(); fmt++) {
                    if (m.code[fmt].op == AbstractOp::END_FORMAT) {
                        break;
                    }
                    if (m.code[fmt].op == AbstractOp::DECLARE_FUNCTION &&
                        m.code[fmt].ref().value().value() == c.right_ref().value().value()) {
                        has_declare_in_fmt = true;
                        break;
                    }
                }
                if (c.op == AbstractOp::DEFINE_ENCODER) {
                    if (!has_declare_in_fmt) {
                        should_bind_encoder[c.right_ref().value().value()] = c.left_ref().value().value();
                    }
                    fmt_to_encoder[c.left_ref().value().value()] = c.right_ref().value().value();
                    encoder_to_fmt[c.right_ref().value().value()] = c.left_ref().value().value();
                }
                else {
                    if (!has_declare_in_fmt) {
                        should_bind_decoder[c.right_ref().value().value()] = c.left_ref().value().value();
                    }
                    fmt_to_decoder[c.left_ref().value().value()] = c.right_ref().value().value();
                    decoder_to_fmt[c.right_ref().value().value()] = c.left_ref().value().value();
                }
            }
        }

        auto add_state_variables = [&](ObjectID fmt) {
            auto ident = m.ident_table_rev.find(fmt);
            if (ident == m.ident_table_rev.end()) {
                return error("Invalid format ident: {}", fmt);
            }
            auto base = ast::as<ast::Format>(ident->second->base.lock());
            if (!base) {
                return error("Invalid format ident: {}", fmt);
            }
            for (auto& s : base->state_variables) {
                auto lock = s.lock();
                if (!lock) {
                    return error("Invalid state variable");
                }
                BM_LOOKUP_IDENT(ident, m, lock->ident);
                rebound.push_back(make_code(AbstractOp::STATE_VARIABLE_PARAMETER, [&](auto& c) {
                    c.ref(ident);
                }));
            }
            return none;
        };

        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_FORMAT) {
                auto ident = c.ident().value().value();
                rebound.push_back(std::move(c));
                auto found_encoder = fmt_to_encoder.find(ident);
                auto found_decoder = fmt_to_decoder.find(ident);
                if (found_encoder != fmt_to_encoder.end()) {
                    rebound.push_back(make_code(AbstractOp::DEFINE_ENCODER, [&](auto& c) {
                        c.left_ref(*varint(ident));
                        c.right_ref(*varint(found_encoder->second));
                    }));
                    if (should_bind_encoder.find(found_encoder->second) != should_bind_encoder.end()) {
                        rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                            c.ref(*varint(found_encoder->second));
                        }));
                    }
                }
                if (found_decoder != fmt_to_decoder.end()) {
                    rebound.push_back(make_code(AbstractOp::DEFINE_DECODER, [&](auto& c) {
                        c.left_ref(*varint(ident));
                        c.right_ref(*varint(found_decoder->second));
                    }));
                    if (should_bind_decoder.find(found_decoder->second) != should_bind_decoder.end()) {
                        rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                            c.ref(*varint(found_decoder->second));
                        }));
                    }
                }
            }
            else if (c.op == AbstractOp::DECLARE_FUNCTION) {
                if (auto found = should_bind_encoder.find(c.ref().value().value()); found != should_bind_encoder.end()) {
                    continue;  // skip; declaration is moved into DEFINE_FORMAT
                }
                if (auto found = should_bind_decoder.find(c.ref().value().value()); found != should_bind_decoder.end()) {
                    continue;  // skip; declaration is moved into DEFINE_FORMAT
                }
                rebound.push_back(std::move(c));
            }
            else if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                // skip
            }
            else if (c.op == AbstractOp::DEFINE_FUNCTION) {
                auto ident = c.ident().value();
                rebound.push_back(std::move(c));
                if (auto found = encoder_to_fmt.find(ident.value()); found != encoder_to_fmt.end()) {
                    rebound.push_back(make_code(AbstractOp::ENCODER_PARAMETER, [&](auto& c) {
                        c.left_ref(*varint(found->second));
                        c.right_ref(ident);
                    }));
                    add_state_variables(found->second);
                }
                if (auto found = decoder_to_fmt.find(ident.value()); found != decoder_to_fmt.end()) {
                    rebound.push_back(make_code(AbstractOp::DECODER_PARAMETER, [&](auto& c) {
                        c.left_ref(*varint(found->second));
                        c.right_ref(ident);
                    }));
                    add_state_variables(found->second);
                }
            }
            else {
                rebound.push_back(std::move(c));
            }
        }
        m.code = std::move(rebound);
        return none;
    }

    void replace_call_encode_decode_ref(Module& m) {
        for (auto& c : m.code) {
            if (c.op == AbstractOp::CALL_ENCODE || c.op == AbstractOp::CALL_DECODE) {
                auto fmt = m.ident_index_table[c.left_ref().value().value()];  // currently this refers to DEFINE_FORMAT
                for (size_t j = fmt; m.code[j].op != AbstractOp::END_FORMAT; j++) {
                    if ((c.op == AbstractOp::CALL_ENCODE && m.code[j].op == AbstractOp::DEFINE_ENCODER) ||
                        (c.op == AbstractOp::CALL_DECODE && m.code[j].op == AbstractOp::DEFINE_DECODER)) {
                        auto ident = m.code[j].right_ref().value();
                        c.left_ref(ident);  // replace to DEFINE_FUNCTION. this DEFINE_FUNCTION holds belong which is original DEFINE_FORMAT
                        break;
                    }
                }
            }
        }
    }

}  // namespace rebgn