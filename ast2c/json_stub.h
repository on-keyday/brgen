/*license*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_handlers json_handlers;
struct json_handlers {
    void* ctx;
    void* (*object_get)(json_handlers*, void*, const char* name);
    void* (*array_get)(json_handlers*, void*, size_t i);
    size_t (*array_size)(json_handlers*, void*);
    char* (*string_get_alloc)(json_handlers*, void*);
    // returns non-zero for success, 0 for error
    int (*number_get)(json_handlers*, void*, size_t* n);
    // returns 0 or 1. -1 for error
    int (*boolean_get)(json_handlers*, void*);
};

#ifdef __cplusplus
}
#endif
