/*license*/
#include "../ast/ast.h"
#include <helper/defer.h>

namespace vm {
    using list = std::list<std::shared_ptr<ast::Object>>;
    struct CallHolder {
        list* holder = nullptr;

        auto enter_holder(list* l) {
            auto old = holder;
            holder = l;
            return utils::helper::defer([&] {
                holder = old;
            });
        }
    };

    void extract_call(CallHolder& h, auto& c) {
        if (ast::Call* call = ast::as<ast::Call>(c)) {
            list args;
            auto l = h.enter_holder(&args);
            extract_call(h, call->callee);
            extract_call(h, call->arguments);
            auto v = std::make_shared<ast::TmpVar>(std::static_pointer_cast<ast::Call>(c));
            c = v;
        }
        ast::traverse();
    }
}  // namespace vm