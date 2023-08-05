#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>

int test_file(std::string_view name) {
    utils::file::View view;
    if (!view.open(name)) {
    }
}

int main() {
}