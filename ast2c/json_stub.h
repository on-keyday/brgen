/*license*/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_handlers json_handlers;
struct json_handlers {
    void* ctx;
    void* (*object_get)(json_handlers*, void* obj, const char* name);
    void* (*array_get)(json_handlers*, void* obj, size_t i);
    size_t (*array_size)(json_handlers*, void* obj);
    int (*is_null)(json_handlers*, void* obj);
    int (*is_array)(json_handlers*, void* obj);
    int (*is_object)(json_handlers*, void* obj);
    char* (*string_get_alloc)(json_handlers*, void* obj);
    const char* (*string_get)(json_handlers*, void* obj);
    // returns non-zero for success, 0 for error
    int (*number_get)(json_handlers*, void* obj, uint64_t* n);
    // returns 0 or 1. -1 for error
    int (*boolean_get)(json_handlers*, void* obj);

    void* (*alloc)(json_handlers*, size_t size, size_t align);
    void (*free)(json_handlers*, void* ptr);
};

#ifdef __cplusplus
}
#endif
