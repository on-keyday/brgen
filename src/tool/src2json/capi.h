/*license*/
#pragma once

#ifndef S2J_EXPORT
#if _WIN32
#define S2J_EXPORT __declspec(dllimport)
#else
#define S2J_EXPORT
#endif
#endif

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

struct Capability {
    bool std_input;
    bool network;
    bool file;
    bool argv;
    bool check_ast;
    bool lexer;
    bool parser;
    bool importer;
    bool ast_json;
};

#ifdef __cplusplus
constexpr auto default_capability = Capability{
    .std_input = true,
    .network = true,
    .file = true,
    .argv = true,
    .check_ast = true,
    .lexer = true,
    .parser = true,
    .importer = true,
    .ast_json = true,
};
#else
#define DEFAULT_CAPABILITY \
    (CAPABILITY) {         \
        .std_input = true, \
        .network = true,   \
        .file = true,      \
        .argv = true,      \
        .check_ast = true, \
        .lexer = true,     \
        .parser = true,    \
        .importer = true,  \
        .ast_json = true   \
    }
#endif

#define S2J_CAPABILITY_STDIN (1 << 0)
#define S2J_CAPABILITY_NETWORK (1 << 1)
#define S2J_CAPABILITY_FILE (1 << 2)
#define S2J_CAPABILITY_ARGV (1 << 3)
#define S2J_CAPABILITY_CHECK_AST (1 << 4)
#define S2J_CAPABILITY_LEXER (1 << 5)
#define S2J_CAPABILITY_PARSER (1 << 6)
#define S2J_CAPABILITY_IMPORTER (1 << 7)
#define S2J_CAPABILITY_AST_JSON (1 << 8)

#define S2J_CAPABILITY_ALL (S2J_CAPABILITY_STDIN | S2J_CAPABILITY_NETWORK | S2J_CAPABILITY_FILE | S2J_CAPABILITY_ARGV | S2J_CAPABILITY_CHECK_AST | S2J_CAPABILITY_LEXER | S2J_CAPABILITY_PARSER | S2J_CAPABILITY_IMPORTER | S2J_CAPABILITY_AST_JSON)
typedef uint64_t CAPABILITY;

#ifdef __cplusplus
constexpr Capability to_capability(CAPABILITY c) {
    Capability cap;
    cap.std_input = (c & S2J_CAPABILITY_STDIN) != 0;
    cap.network = (c & S2J_CAPABILITY_NETWORK) != 0;
    cap.file = (c & S2J_CAPABILITY_FILE) != 0;
    cap.argv = (c & S2J_CAPABILITY_ARGV) != 0;
    cap.check_ast = (c & S2J_CAPABILITY_CHECK_AST) != 0;
    cap.lexer = (c & S2J_CAPABILITY_LEXER) != 0;
    cap.parser = (c & S2J_CAPABILITY_PARSER) != 0;
    cap.importer = (c & S2J_CAPABILITY_IMPORTER) != 0;
    cap.ast_json = (c & S2J_CAPABILITY_AST_JSON) != 0;
    return cap;
}
#endif

typedef void (*out_callback_t)(const char* str, size_t len, bool is_stderr, void* data);
S2J_EXPORT int libs2j_call(int argc, char** argv, CAPABILITY cap, out_callback_t out_callback, void* data);
#ifdef __cplusplus
}
#endif
