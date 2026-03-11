/*license*/
#pragma once
#include <vector>
#include <memory>
#include <ebm/extended_binary_module.hpp>
#include "ebmgen/converter.hpp"
#include "ebmgen/mapping.hpp"

namespace ebmgen {

    struct CFG;
    struct CFGTuple {
        std::shared_ptr<CFG> start;
        std::shared_ptr<CFG> end;
        bool brk = false;  // break the flow
    };

    struct CFGExpression {
        std::weak_ptr<CFGExpression> parent;
        std::string_view relation_name;
        ebm::ExpressionRef original_node;
        std::vector<std::shared_ptr<CFGExpression>> children;
        std::optional<CFGTuple> related_cfg;
    };

    // Control Flow Graph
    // original_node: reference to original statement
    // next: list of next CFG
    // prev: list of previous CFG
    struct CFG {
        ebm::StatementRef original_node;
        std::shared_ptr<CFGExpression> condition;
        std::optional<ebm::StatementKind> statement_op;  // for debug
        std::vector<std::shared_ptr<CFG>> next;
        std::vector<std::weak_ptr<CFG>> prev;
        // for lowered statement representation
        std::vector<CFGTuple> lowered;
    };

    struct DominatorTree {
        std::vector<std::shared_ptr<CFG>> roots;
        std::unordered_map<std::shared_ptr<CFG>, std::shared_ptr<CFG>> parent;
    };

    struct CFGResult {
        CFGTuple cfg;
        DominatorTree dom_tree;
    };

    struct CFGList {
        std::map<std::uint64_t, CFGResult> list;
    };

    struct RepositoryProxy {
       private:
        void* original = nullptr;
        const ebm::Statement* (*get_statement_)(void*, ebm::StatementRef) = nullptr;
        const ebm::Expression* (*get_expression_)(void*, ebm::ExpressionRef) = nullptr;
        const std::vector<ebm::Statement>* all_statement = nullptr;

       public:
        template <class T>
        RepositoryProxy(T* t, const std::vector<ebm::Statement>* all_stmt)
            : original(t), all_statement(all_stmt) {
            get_statement_ = [](void* o, ebm::StatementRef ref) -> const ebm::Statement* {
                return static_cast<T*>(o)->get_statement(ref);
            };
            get_expression_ = [](void* o, ebm::ExpressionRef ref) -> const ebm::Expression* {
                return static_cast<T*>(o)->get_expression(ref);
            };
        }

        const ebm::Statement* get_statement(ebm::StatementRef ref) {
            return get_statement_(original, ref);
        }

        const ebm::Expression* get_expression(ebm::ExpressionRef ref) {
            return get_expression_(original, ref);
        }

        const std::vector<ebm::Statement>* get_all_statement() {
            return all_statement;
        }
    };

    struct CFGStack {
        std::vector<CFGTuple> loop_stack;
        std::shared_ptr<CFG> end_of_function;
        std::map<std::uint64_t, std::shared_ptr<CFG>> cfg_map;
    };

    struct CFGContext {
        TransformContext& tctx;
        CFGStack stack;
    };

    expected<CFGList> analyze_control_flow_graph(CFGStack& stack, RepositoryProxy proxy);
    void write_cfg(futils::binary::writer& w, const CFGList& m, const MappingTable& ctx);
}  // namespace ebmgen
