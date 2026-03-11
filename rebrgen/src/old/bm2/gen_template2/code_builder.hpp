/*license*/
#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace rebgn {

class CodeBuilder {
public:
    void add_line(const std::string& line) {
        lines_.push_back(current_indent_ + line);
    }

    void indent() {
        current_indent_ += "    "; // 4 spaces
    }

    void dedent() {
        if (current_indent_.length() >= 4) {
            current_indent_ = current_indent_.substr(0, current_indent_.length() - 4);
        } else {
            current_indent_ = "";
        }
    }

    std::string build() const {
        std::stringstream ss;
        for (const auto& line : lines_) {
            ss << line << "\n";
        }
        return ss.str();
    }

private:
    std::vector<std::string> lines_;
    std::string current_indent_;
};

} // namespace rebgn

