/*license*/
#include <bm/binary_module.hpp>
#include <bmgen/common.hpp>
#include <bmgen/convert.hpp>
#include <bmgen/internal.hpp>

namespace rebgn {

    struct NestedRanges {
        Range range;
        std::vector<NestedRanges> nested;
    };

    Error extract_ranges(Module& mod, Range range, NestedRanges& current) {
        for (auto i = range.start; i < range.end; i++) {
            auto& c = mod.code[i];
            auto do_extract = [&](AbstractOp begin_op, AbstractOp end_op) {
                auto start = i;
                auto end = i + 1;
                auto nested = 0;
                for (auto j = i + 1; j < range.end; j++) {
                    if (mod.code[j].op == begin_op) {
                        nested++;
                    }
                    else if (mod.code[j].op == end_op) {
                        if (nested == 0) {
                            end = j + 1;
                            break;
                        }
                        nested--;
                    }
                }
                NestedRanges r = {{start, end}};
                // inner extract_ranges
                auto err = extract_ranges(mod, {start + 1, end - 1}, r);
                if (err) {
                    return err;
                }
                current.nested.push_back(std::move(r));
                i = end - 1;
                return none;
            };
            switch (c.op) {
                case AbstractOp::DEFINE_FUNCTION: {
                    auto err = do_extract(AbstractOp::DEFINE_FUNCTION, AbstractOp::END_FUNCTION);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_ENUM: {
                    auto err = do_extract(AbstractOp::DEFINE_ENUM, AbstractOp::END_ENUM);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_FORMAT: {
                    auto err = do_extract(AbstractOp::DEFINE_FORMAT, AbstractOp::END_FORMAT);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_STATE: {
                    auto err = do_extract(AbstractOp::DEFINE_STATE, AbstractOp::END_STATE);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_UNION: {
                    auto err = do_extract(AbstractOp::DEFINE_UNION, AbstractOp::END_UNION);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_UNION_MEMBER: {
                    auto err = do_extract(AbstractOp::DEFINE_UNION_MEMBER, AbstractOp::END_UNION_MEMBER);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_PROGRAM: {
                    auto err = do_extract(AbstractOp::DEFINE_PROGRAM, AbstractOp::END_PROGRAM);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_BIT_FIELD: {
                    auto err = do_extract(AbstractOp::DEFINE_BIT_FIELD, AbstractOp::END_BIT_FIELD);
                    if (err) {
                        return err;
                    }
                    break;
                }
                case AbstractOp::DEFINE_PROPERTY: {
                    auto err = do_extract(AbstractOp::DEFINE_PROPERTY, AbstractOp::END_PROPERTY);
                    if (err) {
                        return err;
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return none;
    }

    expected<AbstractOp> define_to_declare(AbstractOp op) {
        switch (op) {
            case AbstractOp::DEFINE_FUNCTION:
                return AbstractOp::DECLARE_FUNCTION;
            case AbstractOp::DEFINE_ENUM:
                return AbstractOp::DECLARE_ENUM;
            case AbstractOp::DEFINE_FORMAT:
                return AbstractOp::DECLARE_FORMAT;
            case AbstractOp::DEFINE_UNION:
                return AbstractOp::DECLARE_UNION;
            case AbstractOp::DEFINE_UNION_MEMBER:
                return AbstractOp::DECLARE_UNION_MEMBER;
            case AbstractOp::DEFINE_PROGRAM:
                return AbstractOp::DECLARE_PROGRAM;
            case AbstractOp::DEFINE_STATE:
                return AbstractOp::DECLARE_STATE;
            case AbstractOp::DEFINE_BIT_FIELD:
                return AbstractOp::DECLARE_BIT_FIELD;
            case AbstractOp::DEFINE_PROPERTY:
                return AbstractOp::DECLARE_PROPERTY;
            default:
                return unexpect_error("Invalid op: {}", to_string(op));
        }
    }

    Error flatten_ranges(Module& m, NestedRanges& r, std::vector<Code>& flat, std::vector<Code>& outer_nested) {
        size_t outer_range = 0;
        for (size_t i = r.range.start; i < r.range.end; i++) {
            if (outer_range < r.nested.size() &&
                r.nested[outer_range].range.start == i) {
                auto decl = define_to_declare(m.code[i].op);
                if (!decl) {
                    return decl.error();
                }
                flat.push_back(make_code(decl.value(), [&](auto& c) {
                    c.ref(m.code[i].ident().value());
                }));
                std::vector<Code> tmp;
                auto err = flatten_ranges(m, r.nested[outer_range], tmp, outer_nested);
                if (err) {
                    return err;
                }
                auto last = r.nested[outer_range].range.end - 1;
                outer_nested.insert(outer_nested.end(),
                                    std::make_move_iterator(tmp.begin()),
                                    std::make_move_iterator(tmp.end()));
                i = last;
                outer_range++;
            }
            else {
                flat.push_back(std::move(m.code[i]));
            }
        }
        return none;
    }

    Error flatten(Module& m) {
        NestedRanges ranges = {{0, m.code.size()}};
        auto err = extract_ranges(m, ranges.range, ranges);
        if (err) {
            return err;
        }
        std::vector<Code> flat;
        std::vector<Code> outer_nested;
        err = flatten_ranges(m, ranges, flat, outer_nested);
        if (err) {
            return err;
        }
        m.code = std::move(flat);
        m.code.insert(m.code.end(), std::make_move_iterator(outer_nested.begin()), std::make_move_iterator(outer_nested.end()));
        return none;
    }
}  // namespace rebgn
