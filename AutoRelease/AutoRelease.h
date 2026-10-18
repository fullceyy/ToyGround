#pragma once
#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <print>

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
public:
    // 1. Acquire object.
    ~AutoRelease() { D(); }
    AutoRelease() = default;
    AutoRelease(U* pPtr) : Ptr_(pPtr) {}
    
    // 2. Rule of five
    AutoRelease(const AutoRelease& other) = delete;
    AutoRelease& operator=(const AutoRelease& other) = delete;
    
    AutoRelease(AutoRelease&& other) noexcept
    {
        if(&other == *this)
            return;
        
        // Ptr_ = std::exchange(other.Ptr_, nullptr);
        Ptr_ = other.Release(); // both are same?
    }
    
    AutoRelease& operator=(AutoRelease&& other) {}
    
    [[nodiscard]] auto Release() -> U*
    {
        if(Ptr_) 
            Ptr_ = nullptr;

        return Ptr_;
    }
    
    [[nodiscard]] auto Get() const -> const U*
    {
        return Ptr_;
    }
    
    auto operator*() const -> const U&
    {
        if(AutoRelease::Get() == nullptr) 
            throw NullAccessError("AutoRelease operator*(): Dereferencing a nullptr!");
        return *(AutoRelease::Get());
    }
    
    auto operator->() const -> const U*
    {
        if(AutoRelease::Get() == nullptr)
            throw NullAccessError("AutoRelease operator->(): Accessing nullptr!");
        return AutoRelease::Get();
    }
};