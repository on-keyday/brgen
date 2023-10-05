/*license*/
#pragma once
#include "util.h"
#include <escape/escape.h>
#include <concepts>
#include <ranges>
#include <functional>
#include <json/stringer.h>

namespace brgen {

#define sdebugf(name) field(#name, name)
#define sdebugf_omit(name) field(#name, name)

    using JSONWriter = utils::json::Stringer<>;
}  // namespace brgen
