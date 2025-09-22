/*license*/
#pragma once
#include "capi.h"

int src2json_main(int argc, char** argv, const Capability& cap);
#ifdef SRC2JSON_DLL
extern thread_local out_callback_t out_callback;
extern thread_local void* out_callback_data;
#endif
#ifdef __EMSCRIPTEN__
bool js_cancel();
#define may_cancel_task()                              \
    do {                                               \
        if (js_cancel()) {                             \
            print_error("user requested task cancel"); \
            return exit_err;                           \
        }                                              \
    } while (0)
#else
#define may_cancel_task() \
    do {                  \
    } while (0)
#endif
