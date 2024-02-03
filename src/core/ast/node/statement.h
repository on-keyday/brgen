/*license*/
#pragma once
#include "base.h"
#include "expr.h"
#include <vector>
#include "ast_enum.h"

namespace brgen::ast {

    struct StructType;
    struct Function;
    struct IdentType;
    struct Field;

    struct Format : Member {
        define_node_type(NodeType::format);
        std::shared_ptr<IndentBlock> body;
        std::weak_ptr<Function> encode_fn;
        std::weak_ptr<Function> decode_fn;
        std::vector<std::weak_ptr<Function>> cast_fns;
        std::vector<std::weak_ptr<IdentType>> depends;
        std::vector<std::weak_ptr<Field>> state_variables;

        Format(lexer::Loc l)
            : Member(l, NodeType::format) {}

        // for decode
        Format()
            : Member({}, NodeType::format) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(body);
            sdebugf(encode_fn);
            sdebugf(decode_fn);
            sdebugf(cast_fns);
            sdebugf(depends);
            sdebugf(state_variables);
        }
    };

    struct State : Member {
        define_node_type(NodeType::state);
        std::shared_ptr<IndentBlock> body;

        State(lexer::Loc l)
            : Member(l, NodeType::state) {}

        // for decode
        State()
            : Member({}, NodeType::state) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(body);
        }
    };

    struct EnumMember : Member {
        define_node_type(NodeType::enum_member);
        std::shared_ptr<Node> comment;
        std::shared_ptr<Expr> expr;

        EnumMember(lexer::Loc l)
            : Member(l, NodeType::enum_member) {}

        EnumMember()
            : Member({}, NodeType::enum_member) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(expr);
        }
    };

    struct EnumType;

    struct Enum : Member {
        define_node_type(NodeType::enum_);
        lexer::Loc colon_loc;
        scope_ptr scope;
        std::shared_ptr<Type> base_type;
        std::vector<std::shared_ptr<EnumMember>> members;
        std::shared_ptr<EnumType> enum_type;

        std::shared_ptr<EnumMember> lookup(std::string_view ident) {
            for (auto& m : members) {
                if (m->ident->ident == ident) {
                    return m;
                }
            }
            return nullptr;
        }

        Enum(lexer::Loc l)
            : Member(l, NodeType::enum_) {}

        Enum()
            : Member({}, NodeType::enum_) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(scope);
            sdebugf(colon_loc);
            sdebugf(base_type);
            sdebugf(members);
            sdebugf(enum_type);
        }
    };

    struct FieldArgument : Node {
        define_node_type(NodeType::field_argument);
        std::shared_ptr<Expr> raw_arguments;
        lexer::Loc end_loc;
        std::vector<std::weak_ptr<Expr>> collected_arguments;
        // arguments that is passed to encode/decode function (on format type) or fixed value (on integer or floating point type)
        std::vector<std::shared_ptr<Expr>> arguments;
        // alignment of field
        std::shared_ptr<Expr> alignment;
        std::optional<size_t> alignment_value;
        // sub byte range of field
        std::shared_ptr<Expr> sub_byte_length;
        std::shared_ptr<Expr> sub_byte_begin;
        // is_peek is true if field is peeked
        std::shared_ptr<Expr> peek;

        FieldArgument(lexer::Loc l)
            : Node(l, NodeType::field_argument) {}

        FieldArgument()
            : Node({}, NodeType::field_argument) {}

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf_omit(raw_arguments);
            sdebugf(end_loc);
            sdebugf(collected_arguments);
            sdebugf(arguments);
            sdebugf(alignment);
            sdebugf(alignment_value);
            sdebugf(sub_byte_length);
            sdebugf(sub_byte_begin);
        }
    };

    struct Field : Member {
        define_node_type(NodeType::field);
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<FieldArgument> arguments;
        // offset from the beginning of struct
        // if offset is not decidable, offset_bit is std::nullopt
        std::optional<size_t> offset_bit;
        // offset from recent fixed field
        size_t offset_recent = 0;

        std::optional<size_t> tail_offset_bit;
        size_t tail_offset_recent = 0;

        BitAlignment bit_alignment = BitAlignment::not_target;
        Follow follow = Follow::unknown;
        // eventual follow indicates finally followed type
        // for example, format A like below:
        // format A:
        //  a :u8 b :u8
        // follow of a is fixed, but eventual follow is end
        // follow of b is end, and eventual follow is also end
        // for example, format B like below:
        // format B:
        //  a :u8 b :[a]u8
        // follow of a is normal, and eventual follow is also normal
        // follow of b is end, and eventual follow is also end
        // for example, format C like below:
        // format C:
        //  a :u8 b :[a]u8 c :u8 d :[c]u8
        // follow of a is normal, and eventual follow is also normal
        // follow of b is also normal, and eventual follow is also normal
        // follow of c is fixed, and eventual follow is normal
        // follow of d is end, and eventual follow is also end
        // for example, format D like below:
        // format D:
        //  a :u8 b :u16 c :[b]u8 d :u16
        // follow of a is fixed, and eventual follow is normal
        // follow of b is normal, and eventual follow is also normal
        // follow of c is fixed, and eventual follow is end
        // follow of d is end, and eventual follow is also end
        // for example, format E like below:
        // format E:
        //  a :u8 b :u16 c :[..]u8 d :"abs"
        // follow of a is fixed, and eventual follow is normal
        // follow of b is normal, and eventual follow is also normal
        // follow of c is constant, and eventual follow is constant
        // follow of d is end, and eventual follow is also end
        Follow eventual_follow = Follow::unknown;

        Field(lexer::Loc l)
            : Member(l, NodeType::field) {}

        // for decode
        Field()
            : Member({}, NodeType::field) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            // sdebugf(ident);
            sdebugf(colon_loc);
            sdebugf(field_type);
            sdebugf(arguments);
            sdebugf(offset_bit);
            sdebugf(offset_recent);
            sdebugf(tail_offset_bit);
            sdebugf(tail_offset_recent);
            sdebugf(bit_alignment);
            sdebugf(follow);
            sdebugf(eventual_follow);
        }
    };

    struct StructUnionType;

    struct FunctionType;
    struct StructType;

    struct Function : Member {
        define_node_type(NodeType::function);
        std::vector<std::shared_ptr<Field>> parameters;
        std::shared_ptr<Type> return_type;
        std::shared_ptr<IndentBlock> body;
        std::shared_ptr<FunctionType> func_type;
        bool is_cast = false;
        lexer::Loc cast_loc;

        Function(lexer::Loc l)
            : Member(l, NodeType::function) {}

        Function()
            : Member({}, NodeType::function) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(parameters);
            sdebugf(return_type);
            sdebugf(body);
            sdebugf(func_type);
            sdebugf(is_cast);
            sdebugf(cast_loc);
        }
    };

    struct Loop : Stmt {
        define_node_type(NodeType::loop);
        scope_ptr cond_scope;
        std::shared_ptr<Expr> init;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<Expr> step;
        std::shared_ptr<IndentBlock> body;

        Loop(lexer::Loc l)
            : Stmt(l, NodeType::loop) {}

        Loop()
            : Stmt({}, NodeType::loop) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(cond_scope);
            sdebugf(init);
            sdebugf(cond);
            sdebugf(step);
            sdebugf(body);
        }
    };

    struct Return : Stmt {
        define_node_type(NodeType::return_);
        std::shared_ptr<Expr> expr;

        Return(lexer::Loc l)
            : Stmt(l, NodeType::return_) {}

        Return()
            : Stmt({}, NodeType::return_) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(expr);
        }
    };

    struct Break : Stmt {
        define_node_type(NodeType::break_);
        Break(lexer::Loc l)
            : Stmt(l, NodeType::break_) {}

        Break()
            : Stmt({}, NodeType::break_) {}
    };

    struct Continue : Stmt {
        define_node_type(NodeType::continue_);
        Continue(lexer::Loc l)
            : Stmt(l, NodeType::continue_) {}

        Continue()
            : Stmt({}, NodeType::continue_) {}
    };

}  // namespace brgen::ast
