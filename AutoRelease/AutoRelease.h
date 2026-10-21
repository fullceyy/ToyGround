#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>
// #include <print>

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
    ~AutoRelease() { D{}(Ptr_); }
    AutoRelease() : Ptr_(nullptr) {}
    explicit AutoRelease(U* pPtr, D pDeleter) : Ptr_(pPtr), Deleter_(std::move(pDeleter)) {}
    
    AutoRelease(const AutoRelease& other) = delete;
    
    AutoRelease& operator=(const AutoRelease& other) = delete;
    
    AutoRelease(AutoRelease&& other) noexcept
    : Ptr_(other.Release()), Deleter_(std::move(other.Deleter_)) {}
    
    AutoRelease& operator=(AutoRelease&& other) noexcept
    {
        if(&other != this)
        {
            Ptr_ = other.Release();
            Deleter_ = std::move(other.Deleter_);
        }
            return *this;
    }
    
    [[nodiscard]] auto Release()  noexcept -> U*
    {
        return std::exchange(Ptr_, nullptr);
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
    
    friend bool operator==(const AutoRelease& lhs, const AutoRelease& rhs) noexcept
    {
        return lhs.Ptr_ == rhs.Ptr_;
    }
    
    friend bool operator==(const AutoRelease& lhs, std::nullptr_t) noexcept
    {
        return !lhs.Ptr_;
    }
};

// std::make_unique<T>();
template<typename T, typename... Args>
    requires(!std::is_array_v<T>)
auto MakeAutoRelease(Args&&... args)
{
    return AutoRelease<T>(new T(std::forward<Args>(args)...));
}

template<typename T>
    requires std::is_array_v<T>
auto MakeAutoRelease(std::size_t n)
{
    return AutoRelease<T>(new std::remove_extent_t<T>[n]());
}
    