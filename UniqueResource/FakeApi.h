#pragma once
// A fake "C API" that behaves like OpenGL / POSIX handles, with bookkeeping so
// tests can prove nothing leaks and nothing is released twice. No graphics needed.
#include <cstddef>
#include <map>
#include <vector>

#include "UniqueResource.h"

namespace fake
{
// ---- registry: the "driver" -------------------------------------------------
struct Registry
{
    static inline std::map<unsigned, std::size_t> live;   // handle -> size
    static inline unsigned next = 1;                      // 0 is never a real handle (GL style)
    static inline int creates = 0;
    static inline int destroys = 0;
    static inline int doubleFrees = 0;                    // destroy of a handle that isn't live

    static void Reset()
    {
        live.clear();
        next = 1;
        creates = destroys = doubleFrees = 0;
    }
};

// GL style: glCreateBuffers(n, &id) writes the handle through a pointer.
inline void CreateBuffers(int n, unsigned* out)
{
    for (int i = 0; i < n; ++i)
    {
        out[i] = Registry::next++;
        Registry::live[out[i]] = 0;
        ++Registry::creates;
    }
}

inline void DestroyBuffer(unsigned h)
{
    ++Registry::destroys;
    if (Registry::live.erase(h) == 0)
        ++Registry::doubleFrees;
}

// POSIX style: -1 means "no descriptor", 0 is a perfectly valid descriptor.
struct Fds
{
    static inline std::map<int, bool> open;
    static inline int next = 0;
    static inline int doubleCloses = 0;
    static void Reset() { open.clear(); next = 0; doubleCloses = 0; }
};
inline int Open() { int fd = Fds::next++; Fds::open[fd] = true; return fd; }
inline void Close(int fd)
{
    if (Fds::open.erase(fd) == 0)
        ++Fds::doubleCloses;
}
} // namespace fake

// ---- test buffer class: the typical "owner of one handle" -------------------
class Buffer
{
public:
    Buffer() : res_(0u, &fake::DestroyBuffer) {}

    explicit Buffer(std::size_t size) : res_(0u, &fake::DestroyBuffer)
    {
        fake::CreateBuffers(1, &res_);          // out-parameter through operator&
        fake::Registry::live[res_.Get()] = size;
        data_.resize(size);
    }

    Buffer(Buffer&&) noexcept = default;            // rule of zero: UniqueResource does the work
    Buffer& operator=(Buffer&&) noexcept = default;

    auto Handle() const -> unsigned { return res_.Get(); }
    auto Size() const -> std::size_t { return data_.size(); }
    auto Valid() const -> bool { return static_cast<bool>(res_); }
    auto Write(std::size_t i, std::byte b) -> void { data_.at(i) = b; }
    auto Read(std::size_t i) const -> std::byte { return data_.at(i); }
    auto Release() -> void { res_.Reset(0u); }      // explicit early release

private:
    UniqueResource<unsigned> res_;
    std::vector<std::byte> data_;
};

// POSIX-style file descriptor owner (sentinel -1, not 0)
class FileDesc
{
public:
    FileDesc() : res_(-1, &fake::Close) {}
    explicit FileDesc(int fd) : res_(-1, &fake::Close) { res_.Reset(fd); }
    auto Fd() const -> int { return res_.Get(); }
    auto Valid() const -> bool { return static_cast<bool>(res_); }
private:
    UniqueResource<int> res_;
};