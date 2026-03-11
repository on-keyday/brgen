/*license*/
#pragma once

#include "structs.hpp"
namespace ebmcodegen {
    // void generate_class_based(std::string_view ns_name, CodeWriter& header, CodeWriter& source, std::map<std::string_view, Struct>& struct_map, bool is_codegen);
    struct LocationInfo {
        std::string_view location;
        std::string suffix;
    };
    struct IncludeLocations {
        std::string_view ns_name;
        std::vector<LocationInfo> include_locations;
        bool is_codegen = false;
        bool ebmgen_mode = false;

        std::string_view lang;
        std::string_view program_name;
    };
    void generate(const IncludeLocations& locations, CodeWriter& hdr, CodeWriter& src, std::map<std::string_view, Struct>& structs);
}  // namespace ebmcodegen
