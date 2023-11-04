/*license*/
#pragma once
#include <wrap/argv.h>
#include <fnet/util/urlencode.h>
#include <view/slice.h>

int em_main(const char* cmdline, int (*main_)(int, char**)) {
    auto args = utils::view::make_cpy_splitview(cmdline, " ");
    utils::wrap::ArgvVector vec;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string out;
        if (!utils::urlenc::decode(args[i], out)) {
            return 2;
        }
        vec.push_back(std::move(out));
    }
    int argc;
    char** argv;
    vec.arg(argc, argv);
    return main_(argc, argv);
}
