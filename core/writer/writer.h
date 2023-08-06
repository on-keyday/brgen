/*license*/
#pragma once

#include <code/code_writer.h>
#include <string_view>
#include <string>
#include <helper/defer.h>

namespace writer {
    using Writer = utils::code::CodeWriter<std::string, std::string_view>;

    struct TreeWriter {
       private:
        TreeWriter* parent_ = nullptr;
        Writer w_;

       public:
        explicit TreeWriter(TreeWriter* parent)
            : parent_(parent) {}

        TreeWriter* parent() const {
            return parent_;
        }

        Writer& code() {
            return w_;
        }

        ~TreeWriter() {
            if (parent_) {
                parent_->code().write_unformated(w_.out());
            }
        }
    };
}  // namespace writer
