
#include "stream.h"

namespace ast {
    enum class ObjectType {

    };

    struct Object {
        const ObjectType type;

       protected:
        constexpr Object(ObjectType t)
            : type(t) {}
    };
}  // namespace ast
