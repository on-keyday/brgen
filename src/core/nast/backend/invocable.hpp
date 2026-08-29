#include <concepts>

namespace brgen::nast::backend {
    // R への暗黙変換が可能であることを要求するコンセプト
    template <typename F, typename R, typename... Args>
    concept invocable_r = std::invocable<F, Args...> &&
                          (std::same_as<R, void> ||
                           requires(F&& f, Args&&... args) {
                               { std::invoke(static_cast<F&&>(f), static_cast<Args&&>(args)...) } -> std::convertible_to<R>;
                           });
}  // namespace brgen::nast::backend