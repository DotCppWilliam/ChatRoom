#pragma once

#include <list>

namespace util
{
    template <typename T>
    class LinkedList : public std::list<T>
    {
    public:
        ~LinkedList() = default;

        template <typename... Args>
        LinkedList(Args&... args)
            : std::list<T>(std::forward<Args>(args)...) {}
        
        void Append(LinkedList<T>& rhs)
        {
            if (rhs.empty()) return;

            this->insert(this->end(), rhs.begin(), rhs.end());
            rhs.clear();
        }

        template <typename Func>
        inline void Foreach(Func&& func)
        {
            for (auto& item : *this)
                func(item);
        }

        template <typename Func>
        inline void Foreach(Func&& func) const
        {
            for (auto& item : *this)
                func(item);
        }
    };
}