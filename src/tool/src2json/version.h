
#pragma once
#include "capi.h"

#ifndef BRGEN_VERSION
#define BRGEN_VERSION "0.0.0 (unversioned)"
#endif

#ifndef SRC2JSON_VERSION
#define SRC2JSON_VERSION BRGEN_VERSION
#endif

constexpr auto lang_version = BRGEN_VERSION;
constexpr auto src2json_version = SRC2JSON_VERSION;

constexpr auto exit_ok = 0;
constexpr auto exit_err = 1;

int src2json_main(int argc, char** argv, const Capability& cap);

#ifdef S2J_USE_NETWORK
int network_main(const char* port, bool unsafe_escape, const Capability& cap);
#endif
