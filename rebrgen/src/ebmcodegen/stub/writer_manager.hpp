/*license*/
#pragma once
#include <vector>
#include "binary/writer.h"
#include "code/code_writer.h"
#include "ebmgen/common.hpp"
#include <helper/defer.h>

namespace ebmcodegen {
    template <class CodeWriter>
    struct WriterManager;
    template <typename CodeWriter>
    struct WriterWrapper {
       private:
        size_t index = 0;
        // because tmp_writers may be reallocated, we store a reference to the manager instead of a pointer to the writer
        WriterManager<CodeWriter>& manager;

       public:
        CodeWriter& get() {
            assert(index < manager.tmp_writers.size());
            return manager.tmp_writers[index];
        }

        WriterWrapper(WriterManager<CodeWriter>& manager, size_t index)
            : manager(manager), index(index) {}
    };

    template <class CodeWriter>
    struct WriterManager {
        futils::code::CodeWriter<futils::binary::writer&> root;
        std::vector<CodeWriter> tmp_writers;
        [[nodiscard]] auto add_writer() {
            tmp_writers.emplace_back();
            return futils::helper::defer([&]() {
                tmp_writers.pop_back();
            });
        }
        ebmgen::expected<WriterWrapper<CodeWriter>> get_writer() {
            if (tmp_writers.empty()) {
                return ebmgen::unexpect_error("no available writer");
            }
            return WriterWrapper<CodeWriter>(*this, tmp_writers.size() - 1);
        }
    };
}  // namespace ebmcodegen