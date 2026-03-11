/*license*/
#pragma once
#include "code/loc_writer.h"
#include <vector>
#include "ebm/extended_binary_module.hpp"

namespace ebmcodegen::util {
    using CodeWriter = futils::code::LocWriter<std::string, std::vector, ebm::AnyRef>;

}