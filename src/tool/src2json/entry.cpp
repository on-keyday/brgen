/*license*/

#ifdef SRC2JSON_DLL
#include "hook.h"
#include "capi_export.h"
#endif

#include "entry.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../common/em_main.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "../common/print.h"
#include <wrap/argv.h>

#include "version.h"

#ifdef __EMSCRIPTEN__
extern "C" int EMSCRIPTEN_KEEPALIVE emscripten_main(const char* cmdline) {
    Capability cap = default_capability;
    cap.network = false;
    return em_main(cmdline, src2json_main, cap);
}
#elif defined(SRC2JSON_DLL)
bool init_hook() {
    cout.set_hook_write(cout_hook);
    cerr.set_hook_write(cerr_hook);
    return true;
}

thread_local out_callback_t out_callback = nullptr;
thread_local void* out_callback_data = nullptr;

extern "C" int libs2j_call(int argc, char** argv, CAPABILITY cap, out_callback_t cb, void* data) {
    if (argc == 0 || argv == nullptr) {
        return err_invalid;
    }
    static bool init = init_hook();
    if (cb) {
        out_callback = cb;
        out_callback_data = data;
        worker_hook() = [](futils::wrap::UtfOut& out, futils::view::rvec v) -> futils::file::file_result<void> {
            out_callback(v.as_char(), v.size(), &out == &cerr, out_callback_data);
            return {};
        };
    }
    auto cap2 = to_capability(cap);
    return src2json_main(argc, argv, cap2);
}
#else

int main(int argc, char** argv) {
    futils::wrap::U8Arg _(argc, argv);
    cerr.set_virtual_terminal(true);  // ignore error
    cout.set_virtual_terminal(true);  // ignore error
    return src2json_main(argc, argv, default_capability);
}
#endif
