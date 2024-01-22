/*license*/
#pragma once

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

typedef struct Capability {
    bool std_input;
    bool network;
    bool file;
    bool argv;
    bool check_ast;
    bool lexer;
    bool parser;
    bool importer;
    bool std_output;
} CAPABILITY;

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
    .std_output = true,
};
#else
#define DEFAULT_CAPABILITY  \
    (CAPABILITY) {          \
        .std_input = true,  \
        .network = true,    \
        .file = true,       \
        .argv = true,       \
        .check_ast = true,  \
        .lexer = true,      \
        .parser = true,     \
        .importer = true,   \
        .std_output = true, \
    }
#endif

typedef void (*out_callback_t)(const char* str, size_t len, bool is_stderr, void* data);
int libs2j_call(int argc, char** argv, const CAPABILITY* cap, out_callback_t out_callback, void* data);
#ifdef __cplusplus
}
#endif
