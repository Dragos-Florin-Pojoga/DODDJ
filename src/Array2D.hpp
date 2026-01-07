#pragma once

#include <algorithm>
#include <bitset>

enum class StorageOrder { RowMajor, ColumnMajor };

template <typename T, size_t WIDTH, size_t HEIGHT, StorageOrder ORDER = StorageOrder::RowMajor>
class Array2D {
public:
    Array2D() { clear(); }

    T& at(size_t x, size_t y) { return m_data[index(x, y)]; }
    const T& at(size_t x, size_t y) const { return m_data[index(x, y)]; }

private:
    static constexpr size_t index(size_t x, size_t y) {
        if constexpr (ORDER == StorageOrder::RowMajor) {
            return y * WIDTH + x;
        } else {
            return x * HEIGHT + y;
        }
    }

public:

    T& operator()(size_t x, size_t y) { return at(x, y); }
    const T& operator()(size_t x, size_t y) const { return at(x, y); }

    T* begin() { return m_data; }
    const T* begin() const { return m_data; }
    T* end() { return m_data + area(); }
    const T* end() const { return m_data + area(); }

    T* data() { return m_data; }
    const T* data() const { return m_data; }
    size_t width() const { return WIDTH; }
    size_t height() const { return HEIGHT; }
    size_t area() const { return WIDTH * HEIGHT; }

    void fill(const T& value) { std::fill_n(m_data, area(), value); }
    void clear() { std::fill_n(m_data, area(), T()); }

    void copy(const Array2D& other) { std::copy_n(other.m_data, area(), m_data); }
    void swap(Array2D& other) { std::swap(m_data, other.m_data); }

    void operator=(const Array2D& other) { copy(other); }
    void operator=(Array2D&& other) { swap(other); }

private:
    T m_data[WIDTH * HEIGHT];
};

template <size_t WIDTH, size_t HEIGHT>
class Bitset2D {
public:
    Bitset2D() { clear(); }

    bool at(size_t x, size_t y) const { return m_data.test(y * WIDTH + x); }

    bool operator()(size_t x, size_t y) const { return at(x, y); }

    void set(size_t x, size_t y) { m_data.set(y * WIDTH + x); }
    void reset(size_t x, size_t y) { m_data.reset(y * WIDTH + x); }

    bool* begin() { return m_data.begin(); }
    const bool* begin() const { return m_data.begin(); }
    bool* end() { return m_data.end(); }
    const bool* end() const { return m_data.end(); }

    bool* data() { return m_data.data(); }
    const bool* data() const { return m_data.data(); }
    size_t width() const { return WIDTH; }
    size_t height() const { return HEIGHT; }
    size_t area() const { return WIDTH * HEIGHT; }

    void fill() { m_data.set(); }
    void clear() { m_data.reset(); }

    void copy(const Bitset2D& other) { m_data = other.m_data; }
    void swap(Bitset2D& other) { std::swap(m_data, other.m_data); }

    void operator=(const Bitset2D& other) { copy(other); }
    void operator=(Bitset2D&& other) { swap(other); }

private:
    std::bitset<WIDTH * HEIGHT> m_data;
};