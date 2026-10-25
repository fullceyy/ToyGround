#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstddef>

struct NullAccessError : public std::logic_error
{
    using std::logic_error::logic_error;
};

template <typename T, typename D = std::default_delete<T>>
class AutoRelease
{
private:
    using U = std::remove_extent_t<T>;
    U* Ptr_;
    [[no_unique_address]] D Deleter_{};
    
    auto CheckReturn(const char* pMsg) const -> U*
    {
        if(Ptr_ == nullptr) [[unlikely]]
            throw NullAccessError(pMsg);
        return Ptr_;
    }
public:
    ~AutoRelease() { if(Ptr_) Deleter_(Ptr_); } // check needed for after release it may UB.
    
    AutoRelease() : Ptr_(nullptr) {}
    
    explicit AutoRelease(U* pPtr) 
        requires(std::is_default_constructible_v<D> && !std::is_pointer_v<D>)
    : Ptr_(pPtr) {}
    
    explicit AutoRelease(U* pPtr, D pDeleter) : Ptr_(pPtr), Deleter_(std::move(pDeleter)) {}
    
    AutoRelease(const AutoRelease& other) = delete;
    
    AutoRelease& operator=(const AutoRelease& other) = delete;
    
    AutoRelease(AutoRelease&& other) noexcept
    : Ptr_(other.Release()), Deleter_(std::move(other.Deleter_)) {}
    
    AutoRelease& operator=(AutoRelease&& other) noexcept
    { // leaks if this already has something
        if(&other != this)
        {
            Reset(other.Release());
            Deleter_ = std::move(other.Deleter_);
        }
        return *this;
    }
    
    [[nodiscard]] auto Release()  noexcept -> U*
    {
        return std::exchange(Ptr_, nullptr);
    }
    
    // reacquire the pointer and rid of *this Ptr_.
    auto Reset(U* pReset = nullptr) -> void
    {
        if(U* old = std::exchange(Ptr_, pReset)) Deleter_(old); // for move assignment, prevent leak of *this
    }
    
    [[nodiscard]] auto Get() const noexcept -> U*
    {
        return Ptr_;
    }
    
    auto operator*() const -> U&
        requires (!std::is_array_v<T>) // std::is_array<T>::value is same btw.
    {
        return *CheckReturn("AutoRelease operator*(): Dereferencing nullptr!");
    }
    
    auto operator->() const -> U*
        requires(!std::is_array_v<T>)
    {
        return CheckReturn("AutoRelease operator->(): Accessing nullptr!");
    }
    
    auto operator[](std::size_t idx) const -> U&
        requires std::is_array_v<T>
    {
        return CheckReturn("AutoRelease operator[](): Accessing nullptr!")[idx];
    }
    
    explicit operator bool() const noexcept { return (Ptr_ != nullptr); }

    // compares identity to nullptr
    bool operator==(std::nullptr_t) const noexcept
    {
        return !Ptr_;
    }

    // compares identity of the autorelease object
    friend bool operator==(const AutoRelease& lhs, const AutoRelease& rhs) noexcept
    {
        return lhs.Ptr_ == rhs.Ptr_;
    }
    // 
    // friend bool operator!=(const AutoRelease& lhs, const AutoRelease& rhs) noexcept
    // {
    //     return !(lhs.Ptr_ == rhs.Ptr_);
    // }
    
    friend bool operator>=(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator<=(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator>(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator<(const AutoRelease&, const AutoRelease&) = delete;
    friend auto swap(AutoRelease& a, AutoRelease& b) noexcept -> void
    {
        std::swap(a.Ptr_, b.Ptr_);
        std::swap(a.Deleter_, b.Deleter_);
    }
};

// std::make_unique<T>();
template<typename T, typename... Args>
    requires(!std::is_array_v<T>)
auto MakeAutoRelease(Args&&... args)
{
    return AutoRelease<T>(new T(std::forward<Args>(args)...));
}

//  fix for unbounded array
template<typename T>
    requires std::is_unbounded_array_v<T>
auto MakeAutoRelease(std::size_t n)
{
    return AutoRelease<T>(new std::remove_extent_t<T>[n]());
}
