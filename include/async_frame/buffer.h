#ifndef BUFFER_HEADER
#define BUFFER_HEADER

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// it only view of buffer
template <size_t N>
struct StaticBuffer {
    std::array<std::byte, N> buf;
    size_t offset{};
    auto size() noexcept { return buf.size(); }
    auto data() noexcept { return buf.data(); }
    auto capacity() noexcept { return buf.capacity(); }
    auto seek() noexcept { return data() + offset; }
    auto remain() noexcept { return capacity() - offset; }
    auto done() noexcept { return remain() == 0; }
    auto clear() { offset = 0; }
    StaticBuffer &operator+=(size_t of_) noexcept {
        offset += of_;
        return *this;
    }
};

struct DynamicBuffer {
    std::vector<std::byte> buf;
    size_t offset{};
    DynamicBuffer() noexcept : buf(10) {}
    DynamicBuffer(size_t SZ) noexcept : buf(SZ) {}

    auto size() { return buf.size(); }
    auto data() { return buf.data(); }
    auto capacity() { return buf.capacity(); }
    auto seek() { return data() + offset; }
    auto remain() { return capacity() - offset; }
    auto done() noexcept { return remain() == 0; }
    auto read_count() noexcept { return offset; }
    auto clear() { offset = 0; }
    DynamicBuffer &operator+=(size_t N) noexcept {
        offset += N;
        return *this;
    }
};

struct Buffer_View {
    char *data_;
    size_t size_;
    size_t offset{};
    Buffer_View(void *ptr, size_t s) noexcept
        : data_(static_cast<char *>(ptr)), size_(s) {}

    auto size() { return size_; }
    auto data() { return data_; }
    auto seek() { return data_ + offset; }
    auto remain() { return size_ - offset; }
    auto done() noexcept { return remain() == 0; }
    auto read_count() noexcept { return offset; }
    auto clear() { offset = 0; }
    Buffer_View &operator+=(size_t N) noexcept {
        offset += N;

        return *this;
    }
};

#endif
