/*licnese*/
#pragma once
#include <memory>

namespace brgen::nast::backend {
    struct LanguageConfig {
       private:
        const char* (*lang_id)() = nullptr;
        void* lang_v = nullptr;
        template <class T>
        static inline const char* lang_id_fn() {
            return T::lang_name;
        }

       public:
        template <class T>
        void set_language(T& t) {
            lang_v = std::addressof(t);
            lang_id = lang_id_fn<T>;
        }

        const char* lang_name() const {
            if (lang_id) {
                return lang_id();
            }
            return nullptr;
        }

        template <class T>
        T* as() {
            if (lang_id == lang_id_fn<T>) {
                return static_cast<T*>(lang_v);
            }
            return nullptr;
        }
    };

    namespace test {
        struct Conf {
            static constexpr auto lang_name = "dummy";
            int value;
        };
        inline int language_config() {
            LanguageConfig conf;

            Conf c{32};
            conf.set_language(c);
            return conf.as<Conf>()->value;
        }
    }  // namespace test

}  // namespace brgen::nast::backend