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
    
    auto CheckReturn(const char* pMsg) const -> U*
    {
        if(Ptr_ == nullptr) [[unlikely]]
            throw NullAccessError(pMsg);
        return Ptr_;
    }
public:
    // 1. Acquire object.
    ~AutoRelease() { D(); }
    AutoRelease() : Ptr_(nullptr) {}
    AutoRelease(U* pPtr) : Ptr_(pPtr) {}
    
    // 2. Rule of five
    AutoRelease(const AutoRelease& other) = delete;
    AutoRelease& operator=(const AutoRelease& other) = delete;
    
    AutoRelease(AutoRelease&& other) noexcept
    {
        if(&other == this)
            return;
        
        Ptr_ = other.Release(); // both are same?
    }
    
    AutoRelease& operator=(AutoRelease&& other) noexcept
    {
        if(&other == this)
            return *this;
        
        Ptr_ = other.Release();
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
};