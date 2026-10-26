#pragma once
/*
    non copy constructible
    
    The class owns exactly one resource and releases it exactly once.
    Its only job is to make the release happen on every path out of scope,
    including exceptions and early returns.
    It should add nothing else: no reference counting, no value semantics, no implicit magic.

    The philosophy in one line:
    the wrapper is a transparent,
    move-only owner of a value,
    and everything it does is visible in the type.
    
    
    2. Invariants (state these in the header comment)
        Either obj_ == Invalid (empty) or obj_ is a live resource that deleter_ will release exactly once.
        
        A moved-from object is empty.
        
        The deleter is never called on Invalid.
        
        Copying is impossible; moving transfers ownership.
        
        The destructor never throws.
*/
#include <type_traits>
#include <utility>

namespace ToyGround
{
template<typename T, typename D = void (*)(T)>
requires std::is_trivially_copyable_v<T>
class UniqueResource
{
private:
    using SentinelHandle_ = T;
    
    T Res_ {};
    SentinelHandle_ Sentinel_ {0u};
    [[no_unique_address]] D Del_ {};
public:
    ~UniqueResource();
    
    UniqueResource();
    
    UniqueResource(SentinelHandle_ pSent, D pDel);
    
    UniqueResource(const UniqueResource&) = delete;
    
    auto operator=(const UniqueResource&) = delete;
    
    UniqueResource(UniqueResource&& other) noexcept;
    
    auto operator=(UniqueResource&& other) noexcept -> UniqueResource&;
    
    auto Reset(T pNewRes) -> void;
    
    auto Get() const -> T { return Res_; }
    
    operator T() const { return Res_; }
    
    operator bool() const { return Res_ != Sentinel_; }
    
    auto operator&() -> T* { return std::addressof(Res_); }
    
    auto Swap(UniqueResource& other) noexcept -> void;
};

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
UniqueResource<T, D>::~UniqueResource()
{
     if((Res_ != Sentinel_) && Del_)
        Del_(Res_);
}

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
UniqueResource<T, D>::UniqueResource()
{}

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
UniqueResource<T, D>::UniqueResource(SentinelHandle_ pSent, D pDel)
: Res_(pSent), Sentinel_(pSent), Del_(std::move(pDel))
{}

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
UniqueResource<T, D>::UniqueResource(UniqueResource&& other) noexcept
: UniqueResource() // creates UniqueSource of *this.
{
    Swap(other);
}

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
auto UniqueResource<T, D>::operator=(UniqueResource&& other) noexcept -> UniqueResource&
{
        UniqueResource NewUR{std::move(other)};
        Swap(NewUR); // Wow didn't think you can do it this way; and no self-guard!
    // if(std::addressof(other) != this)
    // {
    //     Reset(other.Res_);
    //     other.Res_ = other.Sentinel_;
    //     Del_ = std::move(other.Del_);
    //     Sentinel_ = other.Sentinel_;
    // }
    return *this;
}

template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
auto UniqueResource<T, D>::Swap(UniqueResource& other) noexcept -> void
{
    using std::swap;
    swap(Res_,      other.Res_);
    swap(Del_,      other.Del_);
    swap(Sentinel_, other.Sentinel_);
}


template<typename T, typename D>
requires std::is_trivially_copyable_v<T>
auto UniqueResource<T, D>::Reset(T pRes) -> void
{
    if((Res_ != Sentinel_) && Del_)
        Del_(Res_);
    
    Res_ = pRes;
}

}