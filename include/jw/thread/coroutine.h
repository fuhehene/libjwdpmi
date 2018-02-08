/* * * * * * * * * * * * * * libjwdpmi * * * * * * * * * * * * * */
/* Copyright (C) 2017 J.W. Jagersma, see COPYING.txt for details */
/* Copyright (C) 2016 J.W. Jagersma, see COPYING.txt for details */

#pragma once
#include <jw/thread/task.h>

namespace jw
{
    namespace thread
    {
        namespace detail
        {
            template<typename sig>
            class coroutine_impl;

            template<typename R, typename... A>
            class coroutine_impl<R(A...)> : public task_base
            {
                template<typename> friend class coroutine;
                using this_t = coroutine_impl<R(A...)>;
                using base = task_base;

                std::function<void(this_t&, A...)> function;
                std::unique_ptr<std::tuple<this_t&, A...>> arguments;
                std::optional<R> result;
                bool result_available { false };

            protected:
                virtual void call() override { std::apply(function, *arguments); }

            public:
                // Start the coroutine thread using the specified arguments.
                template <typename... Args>
                constexpr void start(Args&&... args)
                {
                    if (this->is_running()) return; // or throw...?
                    arguments = std::make_unique<std::tuple<this_t&, A...>>(*this, std::forward<Args>(args)...);
                    result.reset();
                    base::start();
                }

                // Blocks until the coroutine yields a result, or terminates.
                // Returns true when it is safe to call await() to obtain the result.
                // May rethrow unhandled exceptions!
                bool try_await()
                {
                    dpmi::throw_if_irq();
                    if (scheduler::is_current_thread(this)) return false;

                    this->try_await_while([this]() { return this->is_running() and not result_available ; });

                    if (!result) return false;
                    return true;
                }

                // Awaits a result from the coroutine.
                // Throws illegal_await if the coroutine ends without yielding a result.
                // May rethrow unhandled exceptions!
                auto await()
                {
                    if (!try_await()) throw illegal_await(this->shared_from_this());

                    result_available = false;
                    this->state = running;
                    return std::move(*result);
                }

                // Called by the coroutine thread to yield a result.
                // This suspends the coroutine until the result is obtained by calling await().
                template <typename... T>
                void yield(T&&... value)
                {
                    if (!scheduler::is_current_thread(this)) return; // or throw?

                    result = std::make_optional<R>(std::forward<T>(value)...);
                    result_available = true;
                    this->state = suspended;
                    ::jw::thread::yield_while([this] { return result_available; });
                    result.reset();
                }

                template <typename F>
                coroutine_impl(F&& f, std::size_t stack_bytes = config::thread_default_stack_size) 
                    : base { stack_bytes }
                    , function { std::forward<F>(f) } { }
            };
        }

        template<typename> class coroutine;

        template<typename R, typename... A>
        class coroutine<R(A...)>
        {
            using task_type = detail::coroutine_impl<R(A...)>;
            std::shared_ptr<task_type> ptr;

        public:
            constexpr const auto get_ptr() const noexcept { return ptr; }
            constexpr auto* operator->() const { return ptr.get(); }
            constexpr auto& operator*() const { return *ptr; }
            constexpr operator bool() const { return ptr.operator bool(); }

            template<typename F>
            constexpr coroutine(F&& f) : ptr(std::make_shared<task_type>(std::forward<F>(f))) { }

            constexpr coroutine(const coroutine&) = default;
            constexpr coroutine() = default;
        };
    }
}
