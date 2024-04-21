#pragma once

#include <functional>
#include <optional>
namespace uitl 
{
    class OnceToken
    {
    public:
        using Task = std::function<void(void)>;

        template <typename Func>
        OnceToken(const Func& on_constructed, Task on_destructed = nullptr)
        {
            on_constructed();
            on_destructed = std::move(on_destructed);
        }

        OnceToken(std::nullopt_t, Task on_destructed = nullptr)
        {
            on_destructed_ = std::move(on_destructed);
        }

        ~OnceToken()
        {
            if (on_destructed_) 
                on_destructed_();
        }
    private:
        Task on_destructed_;
    };
}