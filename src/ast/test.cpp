#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>

void test_file(std::string_view name) {
    utils::file::View view;
    if (!view.open(name)) {
        utils::wrap::cerr_wrap() << name << " cannot open";
        return;
    }
}

int main() {
}