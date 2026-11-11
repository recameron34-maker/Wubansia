#pragma once

// FunctionRef<R (Args...)>: a non-owning reference to a callable, for callbacks that only run during the call they
// are passed to (buildKit's progress, KitCache::acquire's build, readWavInfo's reader). It never allocates, unlike
// std::function, and it must not outlive what it refers to: pass a lambda (or a function) straight into the call.
//
//     buildKit (..., cancel, [&] (int pct) { status.store (pct); });

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace wubansia::engine
{
    template <class Signature>
    class FunctionRef;

    template <class R, class... Args>
    class FunctionRef<R (Args...)>
    {
    public:
        // A callable object (a lambda, a functor, a std::reference_wrapper) invocable as R (Args...).
        template <class F>
            requires (! std::is_same_v<std::remove_cvref_t<F>, FunctionRef> && std::is_object_v<std::remove_reference_t<F>>
                      && std::is_invocable_r_v<R, F&, Args...>)
        FunctionRef (F&& f) noexcept // NOLINT(google-explicit-constructor): passed straight into calls, like std::function
            : invoke ([] (Target t, Args... args) -> R {
                  return std::invoke (*static_cast<std::remove_reference_t<F>*> (t.object), std::forward<Args> (args)...);
              })
        {
            target.object = const_cast<void*> (static_cast<const void*> (std::addressof (f)));
        }

        // A plain function.
        template <class F>
            requires (std::is_function_v<F> && std::is_invocable_r_v<R, F&, Args...>)
        FunctionRef (F& f) noexcept // NOLINT(google-explicit-constructor)
            : invoke ([] (Target t, Args... args) -> R { return std::invoke (reinterpret_cast<F*> (t.function), std::forward<Args> (args)...); })
        {
            target.function = reinterpret_cast<void (*)()> (&f);
        }

        R operator() (Args... args) const { return invoke (target, std::forward<Args> (args)...); }

    private:
        union Target
        {
            void* object;
            void (*function)();
        };

        Target target {};
        R (*invoke) (Target, Args...);
    };
}
