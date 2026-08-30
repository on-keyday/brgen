/*license*/
#pragma once

#include "../../node/code_writer.h"

#define DEFAULT_HANDLER()  \
    if constexpr (false) { \
    }
#define UNHANDLED_ERROR() unexpect_loc_error(node, "Unhandled node: {}#{}", to_string(node.type()), node.id())
#define HANDLE_UNHANDLED()                                      \
    if (ctx.config().unhandled_mode == UnhandledMode::ignore) { \
        return {};                                              \
    }                                                           \
    return UNHANDLED_ERROR();
#define ON_CODEGEN() if constexpr (std::is_same_v<R, CodeWriter> || brgen::nast::has_to_writer<R>)
#define HANDLE_CODEGEN_DEFAULT()                                                                         \
    if (ctx.config().unhandled_mode == UnhandledMode::dummy) {                                           \
        return CODE("{{Unhandled node:", std::format("{}#{}", to_string(node.type()), node.id()), "}}"); \
    }                                                                                                    \
    HANDLE_UNHANDLED();
#define ON_CODEGEN_DEFAULT()      \
    ON_CODEGEN() {                \
        HANDLE_CODEGEN_DEFAULT(); \
    }
#define ON_UNHANDLED() else
#define ON_UNHANDLED_DEFAULT() \
    ON_UNHANDLED() {           \
        HANDLE_UNHANDLED();    \
    }