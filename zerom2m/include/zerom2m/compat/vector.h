/*
 * vector.h
 *
 * ZeroM2M
 * Copyright (C) 2026 ZeroM2M Authors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v3.0 (GPL-3.0).
 */
#pragma once

namespace zerom2m::compat
{

template <typename T> class Vector
{
public:
    Vector() = default;

    Vector(const Vector &other) { *this = other; }

    Vector(Vector &&other) noexcept { *this = static_cast<Vector &&>(other); }

    ~Vector() { delete[] items_; }

    Vector &operator=(const Vector &other)
    {
        if (this == &other) { return *this; }
        clear();
        reserve(other.count_);
        for (size_t i = 0; i < other.count_; ++i) {
            items_[i] = other.items_[i];
        }
        count_ = other.count_;
        return *this;
    }

    Vector &operator=(Vector &&other) noexcept
    {
        if (this == &other) { return *this; }
        delete[] items_;
        items_          = other.items_;
        count_          = other.count_;
        capacity_       = other.capacity_;
        other.items_    = nullptr;
        other.count_    = 0;
        other.capacity_ = 0;
        return *this;
    }

    bool     empty() const { return count_ == 0; }
    size_t   size() const { return count_; }
    unsigned GetCount() const { return static_cast<unsigned>(count_); }

    void clear() { count_ = 0; }

    void reserve(size_t desired)
    {
        if (desired <= capacity_) { return; }
        T *newItems = new T[desired];
        for (size_t i = 0; i < count_; ++i) {
            newItems[i] = items_[i];
        }
        delete[] items_;
        items_    = newItems;
        capacity_ = desired;
    }

    void Append(const T &value) { push_back(value); }

    void push_back(const T &value)
    {
        if (count_ == capacity_) { reserve(capacity_ == 0 ? 4u : capacity_ * 2u); }
        items_[count_++] = value;
    }

    T       &operator[](size_t index) { return items_[index]; }
    const T &operator[](size_t index) const { return items_[index]; }

    T       *begin() { return items_; }
    const T *begin() const { return items_; }
    T       *end() { return items_ + count_; }
    const T *end() const { return items_ + count_; }

private:
    T     *items_{nullptr};
    size_t count_{0};
    size_t capacity_{0};
};

} // namespace zerom2m::compat