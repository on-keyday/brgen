/*license*/
#pragma once

#include <cstddef>
namespace ebmgen {
    template <class Relocate, class ID, class T>
    struct RelocPtr {
       private:
        Relocate* reloc;
        ID id;

       public:
        RelocPtr(Relocate* r, ID i)
            : reloc(r), id(i) {}

        RelocPtr(std::nullptr_t)
            : reloc(nullptr), id() {}

        T* get() {
            if (!reloc) {
                return nullptr;
            }
            return reloc->get(id);
        }

        const T* get() const {
            if (!reloc) {
                return nullptr;
            }
            return reloc->get(id);
        }

        T& operator*() {
            return *get();
        }

        const T& operator*() const {
            return *get();
        }

        T* operator->() {
            return get();
        }

        const T* operator->() const {
            return get();
        }

        explicit operator bool() const {
            return reloc != nullptr;
        }
    };
}  // namespace ebmgen