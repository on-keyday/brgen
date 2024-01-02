
#ifndef BRGEN_VERSION
#define BRGEN_VERSION "0.0.0 (unversioned)"
#endif

#ifndef SRC2JSON_VERSION
#define SRC2JSON_VERSION BRGEN_VERSION
#endif

constexpr auto lang_version = BRGEN_VERSION;
constexpr auto src2json_version = SRC2JSON_VERSION;

int src2json_main(int argc, char** argv);
