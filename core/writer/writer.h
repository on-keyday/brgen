/*license*/
#pragma once

#include <code/code_writer.h>
#include <string_view>
#include <string>
#include <helper/defer.h>

namespace brgen::writer {
    using Writer = utils::code::CodeWriter<std::string, std::string_view>;

    struct TreeWriter {
       private:
        TreeWriter* parent_ = nullptr;
        std::string_view key;
        Writer w_;

       public:
        explicit TreeWriter(std::string_view key, TreeWriter* parent)
            : parent_(parent), key(key) {}

        TreeWriter* parent() const {
            return parent_;
        }

        TreeWriter* lookup(std::string_view k) const {
            if (k == key) {
                return const_cast<TreeWriter*>(this);
            }
            if (parent_) {
                return parent_->lookup(k);
            }
            return nullptr;
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
}  // namespace brgen::writer
