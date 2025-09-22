/*license*/
#pragma once
#include "capi.h"

int src2json_main(int argc, char** argv, const Capability& cap);
#ifdef SRC2JSON_DLL
extern thread_local out_callback_t out_callback;
extern thread_local void* out_callback_data;
#endif