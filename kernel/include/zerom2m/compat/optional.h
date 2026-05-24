/*
 * optional.h
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

template <typename T> class Optional
{
public:
    Optional() = default;
    Optional(const T &value)
        : hasValue_(true)
        , value_(value)
    {
    }
    Optional(T &&value)
        : hasValue_(true)
        , value_(static_cast<T &&>(value))
    {
    }

    Optional(const Optional &)            = default;
    Optional(Optional &&)                 = default;
    Optional &operator=(const Optional &) = default;
    Optional &operator=(Optional &&)      = default;

    Optional &operator=(const T &value)
    {
        hasValue_ = true;
        value_    = value;
        return *this;
    }

    Optional &operator=(T &&value)
    {
        hasValue_ = true;
        value_    = static_cast<T &&>(value);
        return *this;
    }

    bool     has_value() const { return hasValue_; }
    explicit operator bool() const { return hasValue_; }

    T       &value() { return value_; }
    const T &value() const { return value_; }

    T       *operator->() { return &value_; }
    const T *operator->() const { return &value_; }

    T       &operator*() { return value_; }
    const T &operator*() const { return value_; }

    void reset()
    {
        hasValue_ = false;
        value_    = T{};
    }

    template <typename... Args> T &emplace(Args &&...args)
    {
        value_    = T(static_cast<Args &&>(args)...);
        hasValue_ = true;
        return value_;
    }

private:
    bool hasValue_{false};
    T    value_{};
};

} // namespace zerom2m::compat