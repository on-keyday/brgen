/*license*/
#include "json_printer.hpp"

namespace ebmgen {
    JSONPrinter::JSONPrinter(const MappingTable& module)
        : module_(module) {
    }

    template <typename T>
    void JSONPrinter::print_value(const T& value) const {
        if constexpr (std::is_enum_v<T>) {
            os_->value(ebm::to_string(value, true));
        }
        else if constexpr (std::is_same_v<T, bool>) {
            os_->boolean(value);
        }
        else if constexpr (futils::helper::is_template_instance_of<T, std::vector>) {
            auto element = os_->array();
            for (const auto& elem : value) {
                element([&] {
                    print_value(elem);
                });
            }
        }
        else if constexpr (std::is_same_v<T, ebm::Varint>) {
            os_->number(value.value());
        }
        else if constexpr (has_visit<T, DummyFn>) {
            if constexpr (AnyRef<T>) {
                os_->number(get_id(value));
            }
            else {
                auto object = os_->object();
                value.visit([&](auto&& visitor, const char* name, auto&& val) {
                    print_named_value(object, name, val);
                });
            }
        }
        else if constexpr (std::is_integral_v<T>) {
            os_->number(std::uint64_t(value));
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            os_->string(value);
        }
        else {
            static_assert(std::is_same_v<T, void>, "unexpected");
        }
    }

    template <typename T>
    void JSONPrinter::print_named_value(auto&& object, const char* name, const T& value) const {
        object(name, [&] {
            print_value(value);
        });
    }

    template <typename T>
    void JSONPrinter::print_named_value(auto&& object, const char* name, const T* value) const {
        if (value) {
            if constexpr (std::is_same_v<T, char>) {
                print_named_value(object, name, std::string(value));
            }
            else {
                print_named_value(object, name, *value);
            }
        }
    }

    void JSONPrinter::print_module(futils::json::Stringer<>& os) {
        os_ = &os;
        auto origin = module_.module().origin;
        if (origin) {
            print_value(*origin);
        }
    }

    void JSONPrinter ::print_object(futils::json::Stringer<>& os, const ebm::Statement& obj) {
        os_ = &os;
        print_value(obj);
    }
    void JSONPrinter ::print_object(futils::json::Stringer<>& os, const ebm::Expression& obj) {
        os_ = &os;
        print_value(obj);
    }
    void JSONPrinter ::print_object(futils::json::Stringer<>& os, const ebm::Type& obj) {
        os_ = &os;
        print_value(obj);
    }
    void JSONPrinter ::print_object(futils::json::Stringer<>& os, const ebm::Identifier& obj) {
        os_ = &os;
        print_value(obj);
    }
    void JSONPrinter ::print_object(futils::json::Stringer<>& os, const ebm::StringLiteral& obj) {
        os_ = &os;
        print_value(obj);
    }
}  // namespace ebmgen