/*
 * result.hpp
 *
 * Result<T> — a value plus the Status of the operation that produced it. Read
 * operations return this so a failed read is never confused with a real zero
 * (the old getters returned 0 on error, making a dead device look healthy).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <utility>

#include "xmmu/hal/status.hpp"

namespace xmotion::hal {

template <typename T>
struct Result {
  Status status = Status::kOk;
  T value{};

  bool ok() const noexcept { return status == Status::kOk; }
  explicit operator bool() const noexcept { return ok(); }

  const T& operator*() const noexcept { return value; }
  T value_or(T fallback) const { return ok() ? value : std::move(fallback); }

  static Result Ok(T v) { return Result{Status::kOk, std::move(v)}; }
  static Result Err(Status s) { return Result{s, T{}}; }
};

}  // namespace xmotion::hal
