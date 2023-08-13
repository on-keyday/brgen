/*license*/
#pragma once

#include <code/code_writer.h>
#include <string_view>
#include <string>
#include <helper/defer.h>
#include <vector>

namespace brgen::writer {
    using Writer = utils::code::CodeWriter<std::string, std::string_view>;
    /*
    struct Section : std::enable_shared_from_this<Section> {
       private:
        std::weak_ptr<Section> parent;
        std::string key;
        Writer w;
        std::vector<std::shared_ptr<Section>> children;

       public:
        Writer& out() {
            return w;
        }

        std::shared_ptr<Section> find_parent(std::string_view k) {
            if (key == k) {
                return shared_from_this();
            }
            if (auto p = parent.lock()) {
                return p->find_parent(k);
            }
            return nullptr;
        }

        std::shared_ptr<Section> find_child(std::string_view k) {
            for (auto& child : children) {
                if (child->key == k) {
                    return child;
                }
            }
            return nullptr;
        }

        std::shared_ptr<Section> add_child(const std::string& key) {
            auto found = find_child(key);
            if (found) {
                return nullptr;
            }
            auto s = std::make_shared<Section>();
            s->parent = weak_from_this();
            children.push_back(s);
            return s;
        }
    };

    struct SectionWriter {
       private:
        std::shared_ptr<Section> root;

       public:
        std::shared_ptr<Section> current() {}
    };
    */

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

        Writer& out() {
            return w_;
        }

        ~TreeWriter() {
            if (parent_) {
                parent_->out().write_unformatted(w_.out());
            }
        }
    };
}  // namespace brgen::writer
