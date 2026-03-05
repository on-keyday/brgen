#pragma once
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/val.h>
#include <emscripten/bind.h>
#endif

namespace ebmcodegen {
#ifdef __EMSCRIPTEN__
    inline std::uint8_t cancel_flag = 0;
    inline void set_cancel_callback(emscripten::val cb) {
        callback = cb;
    }

    EMSCRIPTEN_BINDINGS(my_module) {
        function("set_cancel_callback", &set_cancel_callback);
    }

    inline bool js_cancel() {
        if (callback.isNull() || callback.isUndefined()) {
            return false;
        }
        auto ret = callback();
        return ret.as<bool>();
    }

#define js_may_cancel_task()                                         \
    do {                                                             \
        if (ebmcodegen::js_cancel()) {                               \
            return ebmgen::unexpect_error("Task cancelled by user"); \
        }                                                            \
    } while (0)
#else
#define js_may_cancel_task() \
    do {                     \
    } while (0)
#endif
}  // namespace ebmcodegen
