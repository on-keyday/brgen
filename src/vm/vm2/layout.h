/*license*/
#pragma once
#include <core/ast/ast.h>

namespace brgen::vm2 {
    enum class LayoutType {
        PTR,        // for nested struct or union
        ARRAY_PTR,  // for dynamic array
        LITERAL,    // for primitive types
    };

    struct Layout {
        std::shared_ptr<ast::Type> type_ast;
        size_t size = 0;
        size_t as_field_size = 0;

       protected:
        Layout() = default;

        Layout(size_t size, size_t as_field_size, const std::shared_ptr<ast::Type>& type_ast)
            : size(size), as_field_size(as_field_size), type_ast(type_ast) {}
    };

    struct Primitive : Layout {
        Primitive(size_t size, size_t as_field_size, const std::shared_ptr<ast::Type>& type_ast)
            : Layout(size, as_field_size, type_ast) {}
    };

    struct Array : Layout {
        std::shared_ptr<Layout> element;
        bool is_dynamic = false;

        Array(size_t size, size_t as_field_size, bool is_dynamic, const std::shared_ptr<ast::Type>& type_ast, const std::shared_ptr<Layout>& element)
            : Layout(size, as_field_size, type_ast), element(element), is_dynamic(is_dynamic) {}
    };
    struct Union;
    struct Struct;

    struct Union : Layout {
        std::vector<std::shared_ptr<Struct>> elements;
    };

    struct Field {
        std::string name;
        std::shared_ptr<Layout> element;
        std::shared_ptr<ast::Field> field;
        size_t offset = 0;

        Field(const std::string& name, const std::shared_ptr<Layout>& element, const std::shared_ptr<ast::Field>& field, size_t offset)
            : name(name), element(element), field(field), offset(offset) {}
    };

    struct Struct : Layout {
        std::vector<std::shared_ptr<Field>> fields;
    };

    struct Derived : Layout {
        std::shared_ptr<Layout> layout;

        Derived(std::shared_ptr<Layout> layout, std::shared_ptr<ast::Type> type_ast)
            : Layout(layout->size, layout->as_field_size, type_ast), layout(layout) {}
    };

    struct TypeContext {
       private:
        std::map<std::shared_ptr<ast::Type>, std::shared_ptr<Layout>> layouts;
        std::map<std::shared_ptr<ast::Field>, std::shared_ptr<Field>> fields;

       public:
        std::shared_ptr<Layout> compute_layout(const std::shared_ptr<ast::Type>& typ);

        std::shared_ptr<Field> get_field(const std::shared_ptr<ast::Field>& field) {
            if (auto it = fields.find(field); it != fields.end()) {
                return it->second;
            }
            return nullptr;
        }
    };

}  // namespace brgen::vm2
