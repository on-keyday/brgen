/*license*/
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <map>

namespace rebgn {

// Forward declarations
class AstRenderer;

// Base AST Node
struct AstNode {
    enum class NodeType {
        Statement,
        Expression,
        Literal,
        BinaryOpExpr,
        UnaryOpExpr,
        VariableDecl,
        FunctionCall,
        ReturnStatement,
        IfStatement,
        SwitchStatement,
        CaseStatement,
        Block,
        ExpressionStatement, // New node type
        LoopStatement,       // New node type
        IndexExpr,           // New node type
        Program,             // New node type for top-level program structure
        // Add more as needed
    };

    NodeType type;
    virtual ~AstNode() = default;
    virtual std::string accept(AstRenderer& renderer) const = 0;
};

// Base Statement Node
struct Statement : AstNode {
    Statement() { type = NodeType::Statement; }
};

// Base Expression Node
struct Expression : AstNode {
    Expression() { type = NodeType::Expression; }
};

// Concrete AST Nodes
struct Literal : Expression {
    std::string value;
    Literal(std::string val) : value(std::move(val)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct BinaryOpExpr : Expression {
    std::unique_ptr<Expression> left;
    std::string op;
    std::unique_ptr<Expression> right;
    BinaryOpExpr(std::unique_ptr<Expression> l, std::string o, std::unique_ptr<Expression> r)
        : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct UnaryOpExpr : Expression {
    std::string op;
    std::unique_ptr<Expression> expr;
    UnaryOpExpr(std::string o, std::unique_ptr<Expression> e)
        : op(std::move(o)), expr(std::move(e)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct VariableDecl : Statement {
    std::string type_name;
    std::string name;
    std::unique_ptr<Expression> initializer;
    VariableDecl(std::string tn, std::string n, std::unique_ptr<Expression> init = nullptr)
        : type_name(std::move(tn)), name(std::move(n)), initializer(std::move(init)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct FunctionCall : Expression {
    std::unique_ptr<Expression> base_object; // For member function calls (e.g., obj.func())
    std::string function_name;
    std::vector<std::unique_ptr<Expression>> arguments;
    // Constructor for free functions
    FunctionCall(std::string name, std::vector<std::unique_ptr<Expression>> args = {})
        : function_name(std::move(name)), arguments(std::move(args)) {}
    // Constructor for member functions
    FunctionCall(std::unique_ptr<Expression> base, std::string name, std::vector<std::unique_ptr<Expression>> args = {})
        : base_object(std::move(base)), function_name(std::move(name)), arguments(std::move(args)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct IndexExpr : Expression {
    std::unique_ptr<Expression> base;
    std::unique_ptr<Expression> index;
    IndexExpr(std::unique_ptr<Expression> b, std::unique_ptr<Expression> i)
        : base(std::move(b)), index(std::move(i)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct ReturnStatement : Statement {
    std::unique_ptr<Expression> expr;
    ReturnStatement(std::unique_ptr<Expression> e = nullptr) : expr(std::move(e)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct IfStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> then_block;
    std::unique_ptr<Statement> else_block;
    IfStatement(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> then_b, std::unique_ptr<Statement> else_b = nullptr)
        : condition(std::move(cond)), then_block(std::move(then_b)), else_block(std::move(else_b)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct SwitchStatement : Statement {
    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Statement>> cases;
    SwitchStatement(std::unique_ptr<Expression> cond, std::vector<std::unique_ptr<Statement>> c = {})
        : condition(std::move(cond)), cases(std::move(c)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct CaseStatement : Statement {
    std::string case_value;
    std::unique_ptr<Statement> body;
    CaseStatement(std::string val, std::unique_ptr<Statement> b)
        : case_value(std::move(val)), body(std::move(b)) {}
    std::string accept(AstRenderer& renderer) const override;
};

struct Block : Statement {
    std::vector<std::unique_ptr<Statement>> statements;
    Block(std::vector<std::unique_ptr<Statement>> stmts = {})
        : statements(std::move(stmts)) {}
    std::string accept(AstRenderer& renderer) const override;
};

// ExpressionStatement
struct ExpressionStatement : Statement {
    std::unique_ptr<Expression> expr;
    ExpressionStatement(std::unique_ptr<Expression> e) : expr(std::move(e)) { type = NodeType::ExpressionStatement; }
    std::string accept(AstRenderer& renderer) const override;
};

// New: LoopStatement
struct LoopStatement : Statement {
    std::unique_ptr<Expression> condition; // nullptr for infinite loop
    std::unique_ptr<Block> body;
    LoopStatement(std::unique_ptr<Block> b, std::unique_ptr<Expression> cond = nullptr)
        : condition(std::move(cond)), body(std::move(b)) { type = NodeType::LoopStatement; }
    std::string accept(AstRenderer& renderer) const override;
};

// AST Renderer
class AstRenderer {
public:
    std::string render(const AstNode& node) {
        current_indent_ = "";
        return node.accept(*this);
    }

    std::string render(const std::unique_ptr<AstNode>& node) {
        if (!node) return "";
        return node->accept(*this);
    }

    std::string render(const std::unique_ptr<Statement>& node) {
        if (!node) return "";
        return node->accept(*this);
    }

    std::string render(const std::unique_ptr<Expression>& node) {
        if (!node) return "";
        return node->accept(*this);
    }

    std::string render(const std::vector<std::unique_ptr<Statement>>& statements) {
        std::stringstream ss;
        for (const auto& stmt : statements) {
            ss << render(stmt);
        }
        return ss.str();
    }

    std::string visit(const Literal& node) {
        return node.value;
    }

    std::string visit(const BinaryOpExpr& node) {
        return "(" + node.left->accept(*this) + " " + node.op + " " + node.right->accept(*this) + ")";
    }

    std::string visit(const UnaryOpExpr& node) {
        return node.op + node.expr->accept(*this);
    }

    std::string visit(const VariableDecl& node) {
        std::string result = current_indent_ + node.type_name + " " + node.name;
        if (node.initializer) {
            result += " = " + node.initializer->accept(*this);
        }
        return result + ";\n";
    }

    std::string visit(const FunctionCall& node) {
        std::string result;
        if (node.base_object) {
            result += node.base_object->accept(*this) + ".";
        }
        result += node.function_name + "(";
        for (size_t i = 0; i < node.arguments.size(); ++i) {
            result += node.arguments[i]->accept(*this);
            if (i < node.arguments.size() - 1) {
                result += ", ";
            }
        }
        return result + ")";
    }

    std::string visit(const IndexExpr& node) {
        return node.base->accept(*this) + "[" + node.index->accept(*this) + "]";
    }

    std::string visit(const ReturnStatement& node) {
        std::string result = current_indent_ + "return";
        if (node.expr) {
            result += " " + node.expr->accept(*this);
        }
        return result + ";\n";
    }

    std::string visit(const IfStatement& node) {
        std::stringstream ss;
        ss << current_indent_ << "if (" << node.condition->accept(*this) << ") {\n";
        indent();
        ss << node.then_block->accept(*this);
        dedent();
        ss << current_indent_ << "}\n";
        if (node.else_block) {
            ss << current_indent_ << "else {\n";
            indent();
            ss << node.else_block->accept(*this);
            dedent();
            ss << current_indent_ << "}\n";
        }
        return ss.str();
    }

    std::string visit(const SwitchStatement& node) {
        std::stringstream ss;
        ss << current_indent_ << "switch (" << node.condition->accept(*this) << ") {\n";
        indent();
        for (const auto& case_stmt : node.cases) {
            ss << case_stmt->accept(*this);
        }
        dedent();
        ss << current_indent_ << "}\n";
        return ss.str();
    }

    std::string visit(const CaseStatement& node) {
        std::stringstream ss;
        ss << current_indent_ << "case " << node.case_value << ": {\n";
        indent();
        ss << node.body->accept(*this);
        dedent();
        ss << current_indent_ << "}\n";
        return ss.str();
    }

    std::string visit(const Block& node) {
        std::stringstream ss;
        for (const auto& stmt : node.statements) {
            ss << stmt->accept(*this);
        }
        return ss.str();
    }

    // New: visit ExpressionStatement
    std::string visit(const ExpressionStatement& node) {
        return current_indent_ + node.expr->accept(*this) + ";\n";
    }

    // New: visit LoopStatement
    std::string visit(const LoopStatement& node) {
        std::stringstream ss;
        ss << current_indent_ << "while (";
        if (node.condition) {
            ss << node.condition->accept(*this);
        } else {
            ss << "true"; // Infinite loop
        }
        ss << ") {\n";
        indent();
        ss << node.body->accept(*this);
        dedent();
        ss << current_indent_ << "}\n";
        return ss.str();
    }

private:
    std::string current_indent_;

    void indent() {
        current_indent_ += "    ";
    }

    void dedent() {
        if (current_indent_.length() >= 4) {
            current_indent_ = current_indent_.substr(0, current_indent_.length() - 4);
        } else {
            current_indent_ = "";
        }
    }
};

// Implement accept methods for concrete nodes
inline std::string Literal::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string BinaryOpExpr::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string UnaryOpExpr::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string VariableDecl::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string FunctionCall::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string ReturnStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string IfStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string SwitchStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string CaseStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string Block::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string ExpressionStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string LoopStatement::accept(AstRenderer& renderer) const { return renderer.visit(*this); }
inline std::string IndexExpr::accept(AstRenderer& renderer) const { return renderer.visit(*this); }

// Helper functions for AST node creation
inline std::unique_ptr<Expression> lit(std::string value) {
    return std::make_unique<Literal>(std::move(value));
}

inline std::unique_ptr<Expression> bin_op(std::unique_ptr<Expression> left, std::string op, std::unique_ptr<Expression> right) {
    return std::make_unique<BinaryOpExpr>(std::move(left), std::move(op), std::move(right));
}

inline std::unique_ptr<Expression> un_op(std::string op, std::unique_ptr<Expression> expr) {
    return std::make_unique<UnaryOpExpr>(std::move(op), std::move(expr));
}

inline std::unique_ptr<Statement> var_decl(std::string type_name, std::string name, std::unique_ptr<Expression> initializer = nullptr) {
    return std::make_unique<VariableDecl>(std::move(type_name), std::move(name), std::move(initializer));
}

template<typename... Args>
inline std::unique_ptr<Expression> func_call(std::string name, Args&&... args) {
    std::vector<std::unique_ptr<Expression>> expr_args;
    (expr_args.push_back(std::forward<Args>(args)), ...);
    return std::make_unique<FunctionCall>(std::move(name), std::move(expr_args));
}

template<typename... Args>
inline std::unique_ptr<Expression> member_func_call(std::unique_ptr<Expression> base, std::string name, Args&&... args) {
    std::vector<std::unique_ptr<Expression>> expr_args;
    (expr_args.push_back(std::forward<Args>(args)), ...);
    return std::make_unique<FunctionCall>(std::move(base), std::move(name), std::move(expr_args));
}

inline std::unique_ptr<Expression> idx_expr(std::unique_ptr<Expression> base, std::unique_ptr<Expression> index) {
    return std::make_unique<IndexExpr>(std::move(base), std::move(index));
}

inline std::unique_ptr<Statement> ret_stmt(std::unique_ptr<Expression> expr = nullptr) {
    return std::make_unique<ReturnStatement>(std::move(expr));
}

inline std::unique_ptr<Statement> if_stmt(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> then_b, std::unique_ptr<Statement> else_b = nullptr) {
    return std::make_unique<IfStatement>(std::move(cond), std::move(then_b), std::move(else_b));
}

inline std::unique_ptr<Block> block(std::vector<std::unique_ptr<Statement>> stmts = {}) {
    return std::make_unique<Block>(std::move(stmts));
}

inline std::unique_ptr<Statement> expr_stmt(std::unique_ptr<Expression> expr) {
    return std::make_unique<ExpressionStatement>(std::move(expr));
}

inline std::unique_ptr<Statement> loop_stmt(std::unique_ptr<Block> body, std::unique_ptr<Expression> cond = nullptr) {
    return std::make_unique<LoopStatement>(std::move(body), std::move(cond));
}

} // namespace rebgn