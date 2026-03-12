/*license*/
#include <bmgen/internal.hpp>

namespace rebgn {
    Error sort_func_in_format(Module& m) {
        std::vector<Code> new_code;
        new_code.reserve(m.code.size());
        for (size_t i = 0; i < m.code.size(); i++) {
            if (m.code[i].op == AbstractOp::DEFINE_FORMAT) {
                std::vector<Code> decl_func;
                while (m.code[i].op != AbstractOp::END_FORMAT) {
                    if (m.code[i].op == AbstractOp::DECLARE_FUNCTION) {
                        decl_func.push_back(std::move(m.code[i]));
                    }
                    else {
                        new_code.push_back(std::move(m.code[i]));
                    }
                    i++;
                }
                new_code.insert_range(new_code.end(), std::move(decl_func));
                new_code.push_back(std::move(m.code[i]));
            }
            else {
                new_code.push_back(std::move(m.code[i]));
            }
        }
        m.code = std::move(new_code);
        return none;
    }
}  // namespace rebgn
