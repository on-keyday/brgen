/*license*/
#pragma once
#include "util.h"
#include <escape/escape.h>
#include <concepts>
#include <ranges>
#include <functional>
#include <json/stringer.h>

namespace brgen {

#define sdebugf(name) field_(#name, name)
#define sdebugf_omit(name) field_(#name, name)

    using JSONWriter = utils::json::Stringer<>;
}  // namespace brgen
