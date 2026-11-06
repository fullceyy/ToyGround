#pragma once

// The resource is owned exclusively and released exactly once.
//
// Invariants:
// - Ptr_ == nullptr means the object is empty.
// - A non-null Ptr_ is owned by this object and released by Deleter_.
// - A moved-from object is empty.
// - The deleter is never called with nullptr.
// - Copying is impossible; moving transfers ownership.
// - The destructor never throws.


#include <memory>
#include <type_traits>
#include <utility>
#include <stdexcept>
#include <cstddef>

namespace ToyGround
{

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
    ~AutoRelease();
    
    AutoRelease();
   
    explicit AutoRelease(U* pPtr) requires(std::is_default_constructible_v<D> && !std::is_pointer_v<D>); // nullptr deleter guard
    
    explicit AutoRelease(U* pPtr, D pDeleter);
    
    AutoRelease(const AutoRelease& other) = delete;
    
    AutoRelease& operator=(const AutoRelease& other) = delete;
   
    AutoRelease(AutoRelease&& other) noexcept;
   
    auto operator=(AutoRelease&& other) noexcept -> AutoRelease&;

    [[nodiscard]] auto Release() noexcept -> U*;

    auto Reset(U* pReset = nullptr) -> void;
    
    [[nodiscard]] auto Get() const noexcept -> U*;

    auto operator*() const -> U& requires(!std::is_array_v<T>);
   
    auto operator->() const -> U* requires(!std::is_array_v<T>);

    auto operator[](std::size_t idx) const -> U& requires std::is_array_v<T>;

    explicit operator bool() const noexcept;

    // compares identity to nullptr
    bool operator==(std::nullptr_t) const noexcept;

    // compares identity of the autorelease object
    friend bool operator==(const AutoRelease& lhs, const AutoRelease& rhs) noexcept
    {
        return lhs.Ptr_ == rhs.Ptr_;
    }
    
    // C++20 makes a!=b to !(a==b), no need to add != overload
    // friend bool operator!=(const AutoRelease& lhs, const AutoRelease& rhs) noexcept
    // {
    //     return !(lhs.Ptr_ == rhs.Ptr_);
    // }
    
    friend bool operator>=(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator<=(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator>(const AutoRelease&, const AutoRelease&) = delete;
    friend bool operator<(const AutoRelease&, const AutoRelease&) = delete;
    
    friend auto Swap(AutoRelease& a, AutoRelease& b) noexcept -> void
    {
        std::swap(a.Ptr_, b.Ptr_);
        std::swap(a.Deleter_, b.Deleter_);
    }
};

template<typename T, typename D>
AutoRelease<T, D>::~AutoRelease()
{
    if(Ptr_) Deleter_(Ptr_);
}

template<typename T, typename D>
AutoRelease<T, D>::AutoRelease()
: Ptr_(nullptr)
{}

template<typename T, typename D>
AutoRelease<T, D>::AutoRelease(U* pPtr)
requires(std::is_default_constructible_v<D> && !std::is_pointer_v<D>)
: Ptr_(pPtr)
{}

template<typename T, typename D>
AutoRelease<T, D>::AutoRelease(U* pPtr, D pDel)
: Ptr_(pPtr), Deleter_(std::move(pDel))
{}
template<typename T, typename D>
AutoRelease<T, D>::AutoRelease(AutoRelease&& other) noexcept
: Ptr_(other.Release()), Deleter_(std::move(other.Deleter_))
{}

template<typename T, typename D>
auto AutoRelease<T, D>::operator=(AutoRelease<T, D>&& other) noexcept -> AutoRelease&
{
    if(this != &other)
    {
        Reset(other.Release());
        Deleter_ = std::move(other.Deleter_);
    }
    return *this;
}

template<typename T, typename D>
auto AutoRelease<T, D>::Release() noexcept -> U*
{
    return std::exchange(Ptr_, nullptr);
}

template<typename T, typename D>
auto AutoRelease<T, D>::Reset(U* pPtr) -> void
{
    if(U* pOld = std::exchange(Ptr_, pPtr)) { Deleter_(pOld); }
}

template<typename T, typename D>
auto AutoRelease<T, D>::Get() const noexcept -> U*
{
    return Ptr_;
}

template<typename T, typename D>
auto AutoRelease<T, D>::operator->() const -> U*
requires(!std::is_array_v<T>)
{
    return CheckReturn("AutoRelease operator->(): Accessing nullptr!");
}

template<typename T, typename D>
auto AutoRelease<T, D>::operator*() const -> U&
requires(!std::is_array_v<T>)
{
    return *CheckReturn("AutoRelease operator*(): Dereferencing nullptr!");
}

template<typename T, typename D>
auto AutoRelease<T, D>::operator[](std::size_t idx) const -> U&
requires std::is_array_v<T>
{
    return CheckReturn("AutoRelease operator[](): Accessing nullptr!")[idx];
}

template<typename T, typename D>
AutoRelease<T, D>::operator bool() const noexcept
{
    return (Ptr_ != nullptr);
}

template<typename T, typename D>
auto AutoRelease<T, D>::operator==(std::nullptr_t) const noexcept -> bool
{
    return !Ptr_; // if !valid -> not true (!= nullptr), if !null -> true (== nullptr)
}

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

}
